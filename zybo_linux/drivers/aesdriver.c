#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <asm/io.h>
#include <linux/of_platform.h>

enum AES_MODE {
	AES_MODE_ENCRYPT = 0,
	AES_MODE_DECRYPT = 0x0000FFFF,
	AES_MODE_SET_KEY = 0xFFFF0000,
};

enum OFFSET {
	OFFSET_DIN  = 0x0,
	OFFSET_DOUT = 0x10,
	OFFSET_CTL  = 0x20,
	OFFSET_STAT = 0x24,
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
	memcpy_toio((u8*)virt_addr + OFFSET_DIN, src, AES_BLOCK_SIZE);
	iowrite32(mode, (u8*)virt_addr + OFFSET_CTL);

	// blocking 
	while (ioread32((u8*)virt_addr + OFFSET_STAT) == 0) { } 
}


void 
aesdriver_read(void *dst)
{
	memcpy_fromio(dst, (u8*)virt_addr + OFFSET_DOUT, AES_BLOCK_SIZE);
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

static int 
aesdriver_cbc_encrypt(struct blkcipher_desc *desc,
			     struct scatterlist *dst, struct scatterlist *src,
			     unsigned int nbytes)
{
	int rv;
	struct blkcipher_walk walk;
	printk(KERN_INFO "Encrypt cbc in aesdriver!\n");
	blkcipher_walk_init(&walk, dst, src, nbytes);
	rv = blkcipher_walk_virt(desc, &walk);
	while ((walk.nbytes)) {
		aesdriver_xor_inplace(walk.src.virt.addr, walk.iv);
		aesdriver_write(walk.src.virt.addr, AES_MODE_ENCRYPT);
		aesdriver_read(walk.dst.virt.addr);
		memcpy(walk.iv, walk.dst.virt.addr, AES_BLOCK_SIZE);
		rv = blkcipher_walk_done(desc, &walk, walk.nbytes - AES_BLOCK_SIZE);
	}
	return rv;
}

static int 
aesdriver_cbc_decrypt(struct blkcipher_desc *desc,
			     struct scatterlist *dst, struct scatterlist *src,
			     unsigned int nbytes)
{
	int rv;
	struct blkcipher_walk walk;
	printk(KERN_INFO "Decrypt cbc in aesdriver!\n");
	blkcipher_walk_init(&walk, dst, src, nbytes);
	rv = blkcipher_walk_virt(desc, &walk);
	while ((walk.nbytes)) {
		aesdriver_write(walk.src.virt.addr, AES_MODE_DECRYPT);
		aesdriver_read(walk.dst.virt.addr);
		aesdriver_xor_inplace(walk.dst.virt.addr, walk.iv);
		memcpy(walk.iv, walk.src.virt.addr, AES_BLOCK_SIZE);
		rv = blkcipher_walk_done(desc, &walk, walk.nbytes - AES_BLOCK_SIZE);
	}
	return rv;
}

static struct crypto_alg aesdriver_cbc_alg = {
	.cra_name		=	"cbc(aes)",
	.cra_driver_name	=	"aesdriver-cbc",
	.cra_priority		=	100,
	.cra_flags		=	CRYPTO_ALG_TYPE_BLKCIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_type		=	&crypto_blkcipher_type,
	.cra_module		=	THIS_MODULE,
	.cra_u			=	{
		.blkcipher = {
			.min_keysize		=	AES_KEYSIZE_128,
			.max_keysize		=	AES_KEYSIZE_128,
			.ivsize	   		= 	AES_BLOCK_SIZE,
			.setkey	   		= 	aesdriver_setkey,
			.encrypt		=	aesdriver_cbc_encrypt,
			.decrypt		=	aesdriver_cbc_decrypt,
		}
	}
};


static int aesdriver_probe(struct platform_device *pdev)
{
	int rv = 0;
	struct resource *res;

	if ((rv = crypto_register_alg(&aesdriver_ecb_alg)))
		goto err;

	if ((rv = crypto_register_alg(&aesdriver_cbc_alg)))
		goto err_unregister_ecb;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "No memory resource\n");
		rv = -ENODEV;
		goto err_unregister_cbc;
	}

	remap_size = res->end - res->start + 1;
	remap_size = 512;
	phys_addr = res->start;
	if (request_mem_region(phys_addr, remap_size, pdev->name) == NULL) {
		dev_err(&pdev->dev, "Cannot request IO\n");
		rv = -ENXIO;
		goto err_unregister_cbc;
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

err_unregister_cbc:
	crypto_unregister_alg(&aesdriver_cbc_alg);

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
	crypto_unregister_alg(&aesdriver_cbc_alg);
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
MODULE_AUTHOR("Brett Nicholas");
MODULE_DESCRIPTION("AES acceleration");
MODULE_LICENSE("DGAF License");

