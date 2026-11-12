#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5063314d, "module_layout" },
	{ 0x46d3d6e4, "platform_driver_unregister" },
	{ 0xa955d91, "__platform_driver_register" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x1e49e99f, "cpu_hwcaps" },
	{ 0x31909811, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x29c1c1c4, "dev_err_probe" },
	{ 0xac49e8e8, "misc_register" },
	{ 0xcee25a0c, "devm_pwm_get" },
	{ 0xf078870e, "devm_gpiod_get" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xd8d38480, "devm_kmalloc" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x287f6da0, "misc_deregister" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0x6188fcf, "_dev_err" },
	{ 0x22ee4d20, "gpiod_set_value" },
	{ 0xc098597f, "pwm_apply_state" },
	{ 0x4b0a3f52, "gic_nonsecure_priorities" },
	{ 0x6881d240, "_dev_info" },
	{ 0x1fdc7df2, "_mcount" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Csmartfan,dc-motor");
MODULE_ALIAS("of:N*T*Csmartfan,dc-motorC*");
