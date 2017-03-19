/*
 *
 * This is a driver for an AES hardware accelerator in PL targeting the 
 * Xilinx Zynq SoC. The actual HDL code for the accelerator is not 
 * included, since it is irrelevant to the course. 
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <asm/io.h>
#include <linux/of_platform.h>

enum AES_MODE {
	AES_MODE_ENCRYPT = 0,
	AES_MODE_DECRYPT = 1,
	AES_MODE_SET_KEY = 2
};

unsigned long remap_size;
unsigned long phys_addr;
unsigned long *virt_addr;

/*
 * Blocking write to driver to handle synchronization
 */
void 
aesdriver_write(const void *src, enum AES_MODE mode)
{
	// write the data to the hardware core through the memory map 
	memcpy_toio((u8*)virt_addr, src, AES_BLOCK_SIZE);
	iowrite32(mode, (u8*)virt_addr);

	// Chill for a bit and let the block do its thing
	while (ioread32((u8*)virt_addr) == 0) { } 
}


void 
aesdriver_read(void *dst)
{
	memcpy_fromio(dst, (u8*)virt_addr, AES_BLOCK_SIZE);
}


static int 
aesdriver_setkey(struct crypto_tfm *tfm, const u8 *in_key, unsigned int key_len)
{
	printk(KERN_INFO "Set key in aesdriver!\n");
	aesdriver_write(in_key, AES_MODE_SET_KEY);
	return 0;
}


void 
aesdriver_xor_inplace(const void *dst, const void *src)
{
	int i;
	for (i = 0; i < 4; ++i)
		*((u32*)dst + i) ^= *((u32*)src + i);
}


/*
 * Encrypt data using hardware 
 */
static int 
aesdriver_ecb_encrypt(struct blkcipher_desc *desc,
			struct scatterlist *dst, 
			struct scatterlist *src,
			unsigned int nbytes)
{
	int rv;
	struct blkcipher_walk walk;
	printk(KERN_INFO "Encrypt ecb in aesdriver!\n");
	blkcipher_walk_init(&walk, dst, src, nbytes);
	rv = blkcipher_walk_virt(desc, &walk);
	while ((walk.nbytes)) {
		aesdriver_write(walk.src.virt.addr, AES_MODE_ENCRYPT);
		aesdriver_read(walk.dst.virt.addr);
		rv = blkcipher_walk_done(desc, &walk, walk.nbytes - AES_BLOCK_SIZE);
	}
	return rv;
}


static int 
aesdriver_ecb_decrypt(struct blkcipher_desc *desc,
		      struct scatterlist *dst, struct scatterlist *src,
		      unsigned int nbytes)
{
	int rv;
	struct blkcipher_walk walk;
	printk(KERN_INFO "Decrypt ecb in aesdriver!\n");
	blkcipher_walk_init(&walk, dst, src, nbytes);
	rv = blkcipher_walk_virt(desc, &walk);
	while ((walk.nbytes)) {
		aesdriver_write(walk.src.virt.addr, AES_MODE_DECRYPT);
		aesdriver_read(walk.dst.virt.addr);
		rv = blkcipher_walk_done(desc, &walk, walk.nbytes - AES_BLOCK_SIZE);
	}
	return rv;
}

static struct crypto_alg aesdriver_ecb_alg = {
		.cra_name		=	"ecb(aes)",
		.cra_driver_name	=	"aesdriver-ecb",
		.cra_priority		=	100,
		.cra_flags		=	CRYPTO_ALG_TYPE_BLKCIPHER,
		.cra_blocksize		=	AES_BLOCK_SIZE,
		.cra_type		=	&crypto_blkcipher_type,
		.cra_module		=	THIS_MODULE,
		.cra_u			=	{
	        .blkcipher = {
			.min_keysize  =  AES_KEYSIZE_128,
			.max_keysize  =	AES_KEYSIZE_128,
			.setkey	      = aesdriver_setkey,
			.encrypt      =	aesdriver_ecb_encrypt,
			.decrypt      =	aesdriver_ecb_decrypt,
		     }
	}
};


static int aesdriver_probe(struct platform_device *pdev)
{
	int rv = 0;
	struct resource *res;

	if ((rv = crypto_register_alg(&aesdriver_ecb_alg)))
		goto err;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "No memory resource\n");
		rv = -ENODEV;
		goto err_unregister_ecb;
	}

	remap_size = res->end - res->start + 1;
	remap_size = 512;
	phys_addr = res->start;
	if (request_mem_region(phys_addr, remap_size, pdev->name) == NULL) {
		dev_err(&pdev->dev, "Cannot request IO\n");
		rv = -ENXIO;
		goto err_unregister_ebc;
	}

	virt_addr = ioremap_nocache(phys_addr, remap_size);
	if (virt_addr == NULL) {
		dev_err(&pdev->dev, "Cannot ioremap memory at 0x%08lx\n", phys_addr);
		rv = -ENOMEM;
		goto err_release_mem;
	}

	printk(KERN_INFO "Probed aesdriver device!\n");
	return rv;

err_release_mem:
	release_mem_region(phys_addr, remap_size);

err_unregister_ecb:
	crypto_unregister_alg(&aesdriver_ecb_alg);

err:
	dev_err(&pdev->dev, "Failed to initialize device\n");
	return rv;
}

static int aesdriver_remove(struct platform_device *pdev)
{
	iounmap(virt_addr);
	release_mem_region(phys_addr, remap_size);
	crypto_unregister_alg(&aesdriver_ecb_alg);
	printk(KERN_INFO "Removed aes accelerator!\n");
	return 0;
}


MODULE_DEVICE_TABLE(of, aesdriver_of_match);

static struct platform_driver aesdriver_platform_driver = {
	.probe = aesdriver_probe,
	.remove = aesdriver_remove,
	.driver = { .name = "aesdriver",
		    .owner = THIS_MODULE,
		    .of_match_table = of_match_ptr(aesdriver_of_match),
	},
};

module_platform_driver(aesdriver_platform_driver);
MODULE_AUTHOR("Brett Nicholas, (heavily) inspired by Lauri Vosandi's code");
MODULE_DESCRIPTION("AES acceleration");
MODULE_LICENSE("DGAF License");

