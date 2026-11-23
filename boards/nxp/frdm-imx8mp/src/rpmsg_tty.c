/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file rpmsg_tty.c
 *
 * Raw RPMsg serial port between the Cortex-M7 (NuttX/PX4) and the
 * Cortex-A53 (Linux).
 *
 * NuttX' own uart_rpmsg driver wraps every payload in a private
 * command/result header and only talks to another NuttX.  The Linux
 * side of the FRDM-IMX8MPLUS uses NXP's imx_rpmsg_tty driver, which
 * binds the "rpmsg-virtual-tty-channel" name-service announcement,
 * creates /dev/ttyRPMSG<n> and moves raw bytes.  This driver speaks that
 * raw protocol and exposes it as a normal NuttX serial device, so PX4
 * modules (MAVLink, uXRCE-DDS, ...) can use it like any UART.
 *
 *  - TX: the xmit ring buffer is sent in RPMsg-sized chunks.  While the
 *    A53 has not bound the channel yet the bytes are discarded, exactly
 *    like a UART with nothing attached (writers never block).
 *  - RX: every received RPMsg is copied into the recv ring buffer; bytes
 *    that do not fit are dropped (no flow control in the Linux driver).
 *  - imx_rpmsg_tty sends "hello world!" once when it binds; that
 *    greeting is swallowed so it never reaches the PX4 protocol parser.
 */

#include <nuttx/config.h>

#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/kmalloc.h>
#include <nuttx/rpmsg/rpmsg.h>
#include <nuttx/serial/serial.h>

#include "board_config.h"

#ifdef CONFIG_BOARD_FRDM_IMX8MP_RPMSG_TTY

#define RPMSG_TTY_GREETING "hello world!"

struct rpmsg_tty_priv_s {
	struct rpmsg_endpoint ept;
	const char *cpuname;
	const char *eptname;
	const uint8_t *rx_data;   /* valid only inside the endpoint callback */
	size_t rx_len;
	bool greeting_seen;
};

static int  rpmsg_tty_setup(struct uart_dev_s *dev) { return OK; }
static void rpmsg_tty_shutdown(struct uart_dev_s *dev) {}
static int  rpmsg_tty_attach(struct uart_dev_s *dev) { return OK; }
static void rpmsg_tty_detach(struct uart_dev_s *dev) {}
static void rpmsg_tty_rxint(struct uart_dev_s *dev, bool enable) {}
static void rpmsg_tty_txint(struct uart_dev_s *dev, bool enable) {}
static void rpmsg_tty_dmarxfree(struct uart_dev_s *dev) {}
static bool rpmsg_tty_txempty(struct uart_dev_s *dev) { return true; }

#ifdef CONFIG_SERIAL_IFLOWCONTROL
static bool rpmsg_tty_rxflowcontrol(struct uart_dev_s *dev, unsigned int nbuffered, bool upper)
{
	return false; /* no flow control: overflowing bytes are dropped */
}
#endif

static void rpmsg_tty_dmasend(struct uart_dev_s *dev)
{
	struct rpmsg_tty_priv_s *priv = dev->priv;
	struct uart_dmaxfer_s *xfer = &dev->dmatx;
	size_t len = xfer->length + xfer->nlength;

	if (is_rpmsg_ept_ready(&priv->ept)) {
		uint32_t space;
		uint8_t *buf = rpmsg_get_tx_payload_buffer(&priv->ept, &space, true);

		if (buf == NULL) {
			xfer->nbytes = 0;
			uart_xmitchars_done(dev);
			return;
		}

		if (len > space) {
			len = space;
		}

		if (len > xfer->length) {
			memcpy(buf, xfer->buffer, xfer->length);
			memcpy(buf + xfer->length, xfer->nbuffer, len - xfer->length);

		} else {
			memcpy(buf, xfer->buffer, len);
		}

		if (rpmsg_send_nocopy(&priv->ept, buf, len) < 0) {
			len = 0;
		}
	}

	/* Not bound yet: drop the bytes (len unchanged) like an unconnected UART */

	xfer->nbytes = len;
	uart_xmitchars_done(dev);
}

static void rpmsg_tty_dmatxavail(struct uart_dev_s *dev)
{
	/* uart_xmitchars_dma() sends one contiguous chunk per call and the
	 * send above completes synchronously, so loop until the ring is empty
	 * (or the link stops accepting data). */

	int guard = 16;

	while (dev->xmit.head != dev->xmit.tail && dev->dmatx.length == 0 && guard-- > 0) {
		uint16_t tail = dev->xmit.tail;
		uart_xmitchars_dma(dev);

		if (dev->xmit.tail == tail) {
			break; /* nothing was consumed */
		}
	}
}

static void rpmsg_tty_dmareceive(struct uart_dev_s *dev)
{
	struct rpmsg_tty_priv_s *priv = dev->priv;
	struct uart_dmaxfer_s *xfer = &dev->dmarx;
	size_t len = priv->rx_len;
	size_t space = xfer->length + xfer->nlength;

	if (len > space) {
		len = space; /* overflow: excess is dropped */
	}

	if (len > xfer->length) {
		memcpy(xfer->buffer, priv->rx_data, xfer->length);
		memcpy(xfer->nbuffer, priv->rx_data + xfer->length, len - xfer->length);

	} else {
		memcpy(xfer->buffer, priv->rx_data, len);
	}

	xfer->nbytes = len;
	uart_recvchars_done(dev);
}

static void rpmsg_tty_send(struct uart_dev_s *dev, int ch)
{
	int nexthead = dev->xmit.head + 1;

	if (nexthead >= dev->xmit.size) {
		nexthead = 0;
	}

	if (nexthead != dev->xmit.tail) {
		dev->xmit.buffer[dev->xmit.head] = ch;
		dev->xmit.head = nexthead;
	}

	rpmsg_tty_dmatxavail(dev);
}

static bool rpmsg_tty_txready(struct uart_dev_s *dev)
{
	int nexthead = dev->xmit.head + 1;

	if (nexthead >= dev->xmit.size) {
		nexthead = 0;
	}

	return nexthead != dev->xmit.tail;
}

static const struct uart_ops_s g_rpmsg_tty_ops = {
	.setup         = rpmsg_tty_setup,
	.shutdown      = rpmsg_tty_shutdown,
	.attach        = rpmsg_tty_attach,
	.detach        = rpmsg_tty_detach,
	.rxint         = rpmsg_tty_rxint,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
	.rxflowcontrol = rpmsg_tty_rxflowcontrol,
#endif
	.dmasend       = rpmsg_tty_dmasend,
	.dmareceive    = rpmsg_tty_dmareceive,
	.dmarxfree     = rpmsg_tty_dmarxfree,
	.dmatxavail    = rpmsg_tty_dmatxavail,
	.send          = rpmsg_tty_send,
	.txint         = rpmsg_tty_txint,
	.txready       = rpmsg_tty_txready,
	.txempty       = rpmsg_tty_txempty,
};

static int rpmsg_tty_ept_cb(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t src, void *priv_)
{
	struct uart_dev_s *dev = ept->priv;
	struct rpmsg_tty_priv_s *priv = dev->priv;

	if (!priv->greeting_seen) {
		priv->greeting_seen = true;

		if (len == sizeof(RPMSG_TTY_GREETING) - 1 && memcmp(data, RPMSG_TTY_GREETING, len) == 0) {
			syslog(LOG_INFO, "[rpmsg_tty] A53 bound %s\n", priv->eptname);
			return 0;
		}
	}

	priv->rx_data = data;
	priv->rx_len = len;
	uart_recvchars_dma(dev);
	priv->rx_data = NULL;
	priv->rx_len = 0;

	return 0;
}

static void rpmsg_tty_device_created(struct rpmsg_device *rdev, void *priv_)
{
	struct uart_dev_s *dev = priv_;
	struct rpmsg_tty_priv_s *priv = dev->priv;

	if (strcmp(priv->cpuname, rpmsg_get_cpuname(rdev)) == 0) {
		priv->greeting_seen = false;
		priv->ept.priv = dev;

		/* Creating a named endpoint sends the name-service announcement
		 * that makes Linux probe imx_rpmsg_tty. */
		int ret = rpmsg_create_ept(&priv->ept, rdev, priv->eptname, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					   rpmsg_tty_ept_cb, NULL);

		if (ret < 0) {
			syslog(LOG_ERR, "[rpmsg_tty] create ept %s failed: %d\n", priv->eptname, ret);
		}
	}
}

static void rpmsg_tty_device_destroy(struct rpmsg_device *rdev, void *priv_)
{
	struct uart_dev_s *dev = priv_;
	struct rpmsg_tty_priv_s *priv = dev->priv;

	if (strcmp(priv->cpuname, rpmsg_get_cpuname(rdev)) == 0) {
		rpmsg_destroy_ept(&priv->ept);
	}
}

int frdm_imx8mp_rpmsg_tty_init(const char *cpuname, const char *eptname, const char *devpath, int bufsize)
{
	struct uart_dev_s *dev = kmm_zalloc(sizeof(struct uart_dev_s));
	struct rpmsg_tty_priv_s *priv = kmm_zalloc(sizeof(struct rpmsg_tty_priv_s));
	int ret = -ENOMEM;

	if (dev == NULL || priv == NULL) {
		goto fail;
	}

	dev->ops       = &g_rpmsg_tty_ops;
	dev->isconsole = false;
	dev->recv.size = bufsize;
	dev->xmit.size = bufsize;
	dev->recv.buffer = kmm_malloc(bufsize);
	dev->xmit.buffer = kmm_malloc(bufsize);

	if (dev->recv.buffer == NULL || dev->xmit.buffer == NULL) {
		goto fail;
	}

	priv->cpuname = cpuname;
	priv->eptname = eptname;
	dev->priv = priv;

	ret = rpmsg_register_callback(dev, rpmsg_tty_device_created, rpmsg_tty_device_destroy, NULL, NULL);

	if (ret < 0) {
		goto fail;
	}

	ret = uart_register(devpath, dev);

	if (ret < 0) {
		rpmsg_unregister_callback(dev, rpmsg_tty_device_created, rpmsg_tty_device_destroy, NULL, NULL);
		goto fail;
	}

	return OK;

fail:

	if (dev) {
		kmm_free(dev->recv.buffer);
		kmm_free(dev->xmit.buffer);
	}

	kmm_free(priv);
	kmm_free(dev);
	return ret;
}

#endif /* CONFIG_BOARD_FRDM_IMX8MP_RPMSG_TTY */
