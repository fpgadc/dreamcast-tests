/* written by gen.cpp: triangles (x, y in 1/16 pixel, z words) and their target pixels; the words refsw3 stores at
   a target with the start at the corner (new) and at the centre with half steps back (old) */
#define NTRI 8
#define NTGT 5
static const case_t CASES[NTRI] = {
    { { -67, 301, 346 }, { 228, 381, -20 }, { 0x40b68422u, 0x40e65c38u, 0x3f9af983u }, { { 20, 0, 0x3fc76f80u, 0x3fc76f60u }, { 18, 4, 0x40242360u, 0x40242340u }, { 15, 8, 0x4066a860u, 0x4066a840u }, { 11, 12, 0x4095a360u, 0x4095a340u }, { 6, 16, 0x40b8ff40u, 0x40b8ff20u }, } },
    { { 185, 294, 166 }, { 492, 274, 377 }, { 0x40a77d2cu, 0x3f2310c0u, 0x40f68500u }, { { 17, 19, 0x3fdbd6e0u, 0x3fdbd760u }, { 16, 21, 0x40191b40u, 0x40191b80u }, { 13, 23, 0x40a3aa80u, 0x40a3aaa0u }, { 11, 25, 0x40da04e0u, 0x40da0500u }, { 12, 26, 0x40b3ad20u, 0x40b3ad40u }, } },
    { { 483, 371, -87 }, { -56, 485, 206 }, { 0x40a2cd73u, 0x4059d7dbu, 0x3ffac5b5u }, { { 18, 7, 0x407627a0u, 0x407627c0u }, { 18, 9, 0x4071aea0u, 0x4071aec0u }, { 17, 13, 0x40642500u, 0x40642520u }, { 4, 15, 0x4023f900u, 0x4023f920u }, { 16, 19, 0x40522260u, 0x40522280u }, } },
    { { 306, 399, 490 }, { 284, 76, -209 }, { 0x3f244d00u, 0x401450f1u, 0x3f951f1bu }, { { 26, 0, 0x3fa93de0u, 0x3fa93f00u }, { 25, 3, 0x3fb9ec20u, 0x3fb9ed40u }, { 24, 5, 0x3f846b80u, 0x3f846ca0u }, { 23, 8, 0x3f9519c0u, 0x3f951ae0u }, { 22, 11, 0x3fa5c820u, 0x3fa5c940u }, } },
    { { 59, -181, -167 }, { 63, 263, -209 }, { 0x40d5ac8cu, 0x3fbe5be8u, 0x3fce636au }, { { 0, 2, 0x40abdae0u, 0x40abdb00u }, { 1, 2, 0x40b71780u, 0x40b717a0u }, { 2, 2, 0x40c25420u, 0x40c25440u }, { 0, 5, 0x40ac7280u, 0x40ac72a0u }, { 1, 5, 0x40b7af20u, 0x40b7af40u }, } },
    { { -201, 386, 382 }, { -193, 341, 62 }, { 0x40e45c9fu, 0x3f843eb2u, 0x406b353au }, { { 10, 0, 0x40952c00u, 0x40952be0u }, { 19, 4, 0x40730420u, 0x40730400u }, { 10, 7, 0x4066a720u, 0x4066a700u }, { 15, 10, 0x404064e0u, 0x404064c0u }, { 18, 14, 0x40142a20u, 0x40142a00u }, } },
    { { 143, 33, -12 }, { -154, 9, 359 }, { 0x3f5223dau, 0x40dc840au, 0x40b4d06eu }, { { 3, 0, 0x40bc76c0u, 0x40bc76a0u }, { 5, 2, 0x40451a20u, 0x40451a00u }, { 3, 5, 0x409ad0a0u, 0x409ad080u }, { 2, 8, 0x40acda20u, 0x40acda00u }, { 2, 12, 0x4091eee0u, 0x4091eec0u }, } },
    { { 459, -189, 186 }, { 230, -143, 379 }, { 0x3fa0818eu, 0x40dd75bcu, 0x3fb4de7fu }, { { 1, 0, 0x409e4540u, 0x409e4560u }, { 15, 11, 0x402940c0u, 0x402940e0u }, { 20, 13, 0x40030620u, 0x40030640u }, { 18, 15, 0x3ffae4e0u, 0x3ffae500u }, { 22, 17, 0x3fb7c3c0u, 0x3fb7c3e0u }, } },
};
