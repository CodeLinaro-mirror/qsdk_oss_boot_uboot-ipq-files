// SPDX-License-Identifier: GPL-2.0+
/*
 * OPTEE Communication Implementation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <asm/io.h>
#include <linux/delay.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <mach/ipq.h>
#include <cpu_func.h>
#include <command.h>
#include <errno.h>
#include <tee.h>
#include <vsprintf.h>

/* OPTEE fuseipq implementation */
#define PTA_CMD_QFPROM_BLOW_SECELF 0
#define TA_FUSEIPQ_UUID \
	{ 0x7e5e8c5d, 0x5375, 0x4ab0, \
	{ 0x8b, 0x11, 0x95, 0x14, 0x96, 0x65, 0xdc, 0x88} }

int ipq_fuseipq_optee_impl(void *params)
{
	const struct tee_optee_ta_uuid uuid = TA_FUSEIPQ_UUID;
	struct tee_open_session_arg session_arg;
	struct udevice *dev = NULL;
	struct tee_shm *shm_elf, *shm_dat;
	struct tee_invoke_arg arg;
	struct tee_param param[4];
	struct fuseipq_params *fuseipq_params = (struct fuseipq_params *)params;
	int ret = 0;

	dev = tee_find_device(dev, NULL, NULL, NULL);
	if (!dev) {
		printf("tee_find_session(): failed(%d)\n", ret);
		return -ENODEV;
	}

	memset(&session_arg, 0, sizeof(session_arg));
	tee_optee_ta_uuid_to_octets(session_arg.uuid, &uuid);

	ret = tee_open_session(dev, &session_arg, 0, NULL);
	if (ret) {
		printf("tee_open_session(): failed(%d)\n", ret);
		return ret;
	}

	memset(&arg, 0, sizeof(arg));
	memset(param, 0, sizeof(param));

	arg.func = PTA_CMD_QFPROM_BLOW_SECELF;
	arg.session = session_arg.session;

	ret = tee_shm_register(dev, (void *)(ulong)fuseipq_params->addr,
			       fuseipq_params->meta_data_size, 0x0, &shm_elf);
	if (ret < 0) {
		printf("Cannot register input elf memory 0x%X\n", ret);
		goto error;
	}

	param[0].u.memref.shm	= shm_elf;
	param[0].u.memref.size	= shm_elf->size;
	param[0].attr		= TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;

	ret = tee_shm_register(dev, (void *)(ulong)fuseipq_params->load_seg_buff,
			       fuseipq_params->load_seg_cnt, 0x0, &shm_dat);
	if (ret < 0) {
		printf("Cannot register input dat memory 0x%X\n", ret);
		tee_shm_free(shm_elf);
		goto error;
	}

	param[1].u.memref.shm	= shm_dat;
	param[1].u.memref.size	= shm_dat->size;
	param[1].attr		= TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;

	param[2].attr = TEE_PARAM_ATTR_TYPE_VALUE_OUTPUT;
	param[3].attr = TEE_PARAM_ATTR_TYPE_NONE;

	ret = tee_invoke_func(dev, &arg, 4, param);
	if (ret) {
		printf("tee_invoke_func(): failed(%d)\n", ret);
		tee_shm_free(shm_elf);
		tee_shm_free(shm_dat);
		goto error;
	}

	fuseipq_params->fuse_status = param[2].u.value.a;
	tee_shm_free(shm_elf);
	tee_shm_free(shm_dat);
error:
	ret = tee_close_session(dev, arg.session);
	if (ret < 0)
		printf("tee_close_session(): failed(%d)\n", ret);

	return ret;
}
