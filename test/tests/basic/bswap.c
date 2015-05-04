//test return -65536
// Tests basic function call

unsigned static short __bswap_16(unsigned short __bsx)
{
    return ((unsigned short)((((__bsx) >> 8) & 255) | (((__bsx) & 255) << 8)));
}


unsigned static int __bswap_32(unsigned int __bsx)
{
    return ((((__bsx) & -16777216) >> 24) | (((__bsx) & 16711680) >> 8) | (((__bsx) & 65280) << 8) | (((__bsx) & 255) << 24));
}


int __test() {
    unsigned x = __bswap_32(0xFFFF);
    return x;
}
