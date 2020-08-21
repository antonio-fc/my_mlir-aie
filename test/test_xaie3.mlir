// Note: This test *might* fail due to the random order that the code statements are generated

// RUN: aie-translate --aie-generate-xaie %s | FileCheck %s

// CHECK: XAieTile_CoreControl(&(TileInst[3][3]), XAIE_ENABLE, XAIE_DISABLE);
// CHECK: XAieTile_CoreControl(&(TileInst[3][3]), XAIE_ENABLE, XAIE_DISABLE);
// CHECK: XAieDma_TileInitialize(&(TileInst[3][3]), &(TileDMAInst[3][3]));
// CHECK: XAieDma_TileBdClearAll(&(TileDMAInst[3][3]));
// CHECK: XAieDma_TileChResetAll(&(TileDMAInst[3][3]));
// CHECK: XAieDma_TileBdSetLock(&(TileDMAInst[3][3]), 0, XAIEDMA_TILE_BD_ADDRA, 0, XAIE_ENABLE, 0, XAIE_ENABLE, 1);
// CHECK: XAieDma_TileBdSetAdrLenMod(&(TileDMAInst[3][3]), 0, 0, 0, 256, XAIE_ENABLE, XAIE_DISABLE);
// CHECK: XAieDma_TileBdWrite(&(TileDMAInst[3][3]), 0);
// CHECK: XAieDma_TileSetStartBd(&(TileDMAInst[3][3]), XAIEDMA_TILE_CHNUM_MM2S0, 0);
// CHECK: XAieDma_TileChControl(&(TileDMAInst[3][3]), XAIEDMA_TILE_CHNUM_MM2S0, XAIE_RESETDISABLE, XAIE_ENABLE);
// CHECK: XAieDma_TileInitialize(&(TileInst[3][3]), &(TileDMAInst[3][3]));
// CHECK: XAieDma_TileBdClearAll(&(TileDMAInst[3][3]));
// CHECK: XAieDma_TileChResetAll(&(TileDMAInst[3][3]));
// CHECK: XAieDma_TileBdSetLock(&(TileDMAInst[3][3]), 0, XAIEDMA_TILE_BD_ADDRA, 0, XAIE_ENABLE, 1, XAIE_ENABLE, 0);
// CHECK: XAieDma_TileBdSetAdrLenMod(&(TileDMAInst[3][3]), 0, 0, 0, 16, XAIE_ENABLE, XAIE_DISABLE);
// CHECK: XAieDma_TileBdSetLock(&(TileDMAInst[3][3]), 1, XAIEDMA_TILE_BD_ADDRA, 1, XAIE_ENABLE, 1, XAIE_ENABLE, 0);
// CHECK: XAieDma_TileBdSetAdrLenMod(&(TileDMAInst[3][3]), 1, 0, 0, 16, XAIE_ENABLE, XAIE_DISABLE);
// CHECK: XAieDma_TileBdSetLock(&(TileDMAInst[3][3]), 2, XAIEDMA_TILE_BD_ADDRA, 2, XAIE_ENABLE, 1, XAIE_ENABLE, 0);
// CHECK: XAieDma_TileBdSetAdrLenMod(&(TileDMAInst[3][3]), 2, 0, 0, 16, XAIE_ENABLE, XAIE_DISABLE);
// CHECK: XAieDma_TileBdSetLock(&(TileDMAInst[3][3]), 3, XAIEDMA_TILE_BD_ADDRA, 3, XAIE_ENABLE, 1, XAIE_ENABLE, 0);
// CHECK: XAieDma_TileBdSetAdrLenMod(&(TileDMAInst[3][3]), 3, 0, 0, 16, XAIE_ENABLE, XAIE_DISABLE);
// CHECK: XAieDma_TileBdWrite(&(TileDMAInst[3][3]), 2);
// CHECK: XAieDma_TileBdSetNext(&(TileDMAInst[3][3]), 2, 3);
// CHECK: XAieDma_TileBdWrite(&(TileDMAInst[3][3]), 0);
// CHECK: XAieDma_TileBdSetNext(&(TileDMAInst[3][3]), 0, 1);
// CHECK: XAieDma_TileBdWrite(&(TileDMAInst[3][3]), 1);
// CHECK: XAieDma_TileBdSetNext(&(TileDMAInst[3][3]), 1, 2);
// CHECK: XAieDma_TileBdWrite(&(TileDMAInst[3][3]), 3);
// CHECK: XAieDma_TileSetStartBd(&(TileDMAInst[3][3]), XAIEDMA_TILE_CHNUM_S2MM0, 0);
// CHECK: XAieDma_TileChControl(&(TileDMAInst[3][3]), XAIEDMA_TILE_CHNUM_S2MM0, XAIE_RESETDISABLE, XAIE_ENABLE);

module @test_xaie3 {
  %t33 = AIE.tile(3, 3)
  %t44 = AIE.tile(4, 4)

  %buf33_0 = AIE.buffer(%t33) : memref<256xi32>
  %buf44_0 = AIE.buffer(%t44) : memref<64xi32>
  %buf44_1 = AIE.buffer(%t44) : memref<64xi32>
  %buf44_2 = AIE.buffer(%t44) : memref<64xi32>
  %buf44_3 = AIE.buffer(%t44) : memref<64xi32>

  %l33_0 = AIE.lock(%t33, 0)
  %l44_0 = AIE.lock(%t44, 0)
  %l44_1 = AIE.lock(%t44, 1)
  %l44_2 = AIE.lock(%t44, 2)
  %l44_3 = AIE.lock(%t44, 3)

  %m33 = AIE.mem(%t33) {
    %srcDma = AIE.dmaStart("MM2S0")
    AIE.terminator(^dma0, ^end)
    ^dma0:
      cond_br %srcDma, ^bd0, ^end
    ^bd0:
      AIE.useLock(%l33_0, "Acquire", 1, 0)
      AIE.dmaBd(<%buf33_0 : memref<256xi32>, 0, 256>, 0)
      AIE.useLock(%l33_0, "Release", 0, 0)
      br ^end
    ^end:
      AIE.end
  }

  %m44 = AIE.mem(%t44) {
    %dstDma = AIE.dmaStart("S2MM0")
    AIE.terminator(^dma0, ^end)
    ^dma0:
      cond_br %dstDma, ^bd0, ^end
    ^bd0:
      AIE.useLock(%l44_0, "Acquire", 0, 0)
      AIE.dmaBd(<%buf44_0 : memref<64xi32>, 0, 16>, 0)
      AIE.useLock(%l44_0, "Release", 1, 0)
      br ^bd1
    ^bd1:
      AIE.useLock(%l44_1, "Acquire", 0, 0)
      AIE.dmaBd(<%buf44_1 : memref<64xi32>, 0, 16>, 0)
      AIE.useLock(%l44_1, "Release", 1, 0)
      br ^bd2
    ^bd2:
      AIE.useLock(%l44_2, "Acquire", 0, 0)
      AIE.dmaBd(<%buf44_2 : memref<64xi32>, 0, 16>, 0)
      AIE.useLock(%l44_2, "Release", 1, 0)
      br ^bd3
    ^bd3:
      AIE.useLock(%l44_3, "Acquire", 0, 0)
      AIE.dmaBd(<%buf44_3 : memref<64xi32>, 0, 16>, 0)
      AIE.useLock(%l44_3, "Release", 1, 0)
      br ^end
    ^end:
      AIE.end
  }
}