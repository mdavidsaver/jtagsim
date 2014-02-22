#include <linux/module.h>
#include <linux/init.h>

static int __init ppsim_init(void)
{
    return 0;
}
module_init(ppsim_init);

static void __exit ppsim_exit(void)
{
}
module_exit(ppsim_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Davidsaver <mdavidsaver@gmail.com");
MODULE_DESCRIPTION("Simulated Urjtag minimal parallel port programmer");
