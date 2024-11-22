//===- AIETargetXAIEV2.cpp --------------------------------------*- C++ -*-===//
//
// This file is licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// (c) Copyright 2021 Xilinx Inc.
// (c) Copyright 2021-2023, Advanced Micro Devices, Inc.
//
//===----------------------------------------------------------------------===//
#include "AIETargetShared.h"

#include "aie/Dialect/AIE/IR/AIEDialect.h"
#include "aie/Dialect/AIEX/IR/AIEXDialect.h"
#include "aie/Targets/AIETargets.h"

#include "mlir/IR/Attributes.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/mlir-translate/MlirTranslateMain.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Module.h"

using namespace mlir;
using namespace xilinx;
using namespace xilinx::AIE;
using namespace xilinx::AIEX;

namespace xilinx::AIE {

// This string is output at the top of the lowered C++ code.
const char *xaie_cpp_file_header = R"code(
// This file was auto-generated by aiecc.py --aie-generate-xaie.

#ifndef MLIR_AIE_QUIET
#define __mlir_aie_verbose(x) x
#else
#define __mlir_aie_verbose(x)
#endif

// The following is a wrapper for the common "if(call() != 0) return 1" pattern.
// Use this only in functions that return int. If the call this wrapper is used
// on does not succeed, the expanded code will exit out of the function
// containing this macro with an error code.
#define __mlir_aie_try(x) do { \
  AieRC ret = (x); \
  if(ret != XAIE_OK) { \
    return x; \
  } \
} while(0)

static XAie_DmaDimDesc *__mlir_aie_alloc_dim_desc(size_t ndims) {
  XAie_DmaDimDesc *ret = NULL;
  ret = (XAie_DmaDimDesc *)calloc(sizeof(XAie_DmaDimDesc), ndims);
  if(NULL == ret) {
    __mlir_aie_verbose(fprintf(stderr, "Allocating DmaDimDesc failed.\n"));
  }
  return ret;
}

)code";

static std::string tileLockStr(StringRef id, StringRef val) {
  std::string str;
  llvm::raw_string_ostream rss(str);
  // rss << "XAie_Lock(" << id << "," << val << ")";
  rss << "XAie_LockInit(" << id << "," << val << ")";
  return str;
}

// FIXME: code bloat. this shouldn't really be a template, but need
// a proper DMA-like interface
// blockMap: A map that gives a unique bd ID assignment for every block.
template <typename OpType>
mlir::LogicalResult generateDMAConfig(OpType memOp, raw_ostream &output,
                                      const AIETargetModel &targetModel,
                                      DenseMap<Block *, int> blockMap) {
  StringRef enable = "XAIE_ENABLE";
  StringRef disable = "XAIE_DISABLE";
  StringRef deviceInstRef = "&(ctx->DevInst)"; // TODO

  int col = memOp.colIndex();
  int row = memOp.rowIndex();

  for (auto &block : memOp.getBody()) {
    bool foundBdPacket = false;
    int packetType = 0;
    int packetID = 0;
    bool foundBd = false;
    int lenA = 0;
    int offsetA = 0;
    int BaseAddrA = 0;
    int elementWidthInBytes = 0;
    int ndims = 0;
    ArrayRef<BDDimLayoutAttr> dims;
    //      StringRef FifoMode = disable; // FIXME: when to enable FIFO mode?
    for (auto op : block.template getOps<DMABDOp>()) {
      foundBd = true;
      if (!targetModel.isShimNOCTile(col, row)) {
        assert(op.getBufferOp().getAddress() &&
               "buffer must have address assigned");
        BaseAddrA = op.getBufferOp().getAddress().value();
        int bufferCol = op.getBufferOp().getTileOp().colIndex();
        int bufferRow = op.getBufferOp().getTileOp().rowIndex();

        // Memtile DMAs can access neighboring tiles.
        if (targetModel.isMemTile(col, row)) {
          if (targetModel.isWest(col, row, bufferCol, bufferRow))
            BaseAddrA += 0x0;
          else if (targetModel.isInternal(col, row, bufferCol, bufferRow))
            BaseAddrA += targetModel.getMemTileSize() * 1;
          else if (targetModel.isEast(col, row, bufferCol, bufferRow))
            BaseAddrA += targetModel.getMemTileSize() * 2;
        }
      }

      lenA = op.getLenInBytes();
      offsetA = op.getOffsetInBytes();
      elementWidthInBytes = op.getBufferElementTypeWidthInBytes();
      if (op.getDimensions()) {
        dims = *op.getDimensions();
        ndims = dims.size();
      }
    }

    if (0 != ndims)
      if (!targetModel.hasProperty(AIETargetModel::UsesMultiDimensionalBDs))
        return memOp.emitOpError("DMA contains at least one multi-dimensional "
                                 "buffer descriptor. This is currently only "
                                 "supported for AIE-ML and later devices.");

    int acqValue = 0, relValue = 0;
    bool hasAcq = false, hasRel = false;
    int acqLockID = 0, relLockID = 0;
    for (auto op : block.template getOps<UseLockOp>()) {
      LockOp lock = cast<LockOp>(op.getLock().getDefiningOp());
      int lockCol = lock.colIndex();
      int lockRow = lock.rowIndex();
      int lockID = lock.getLockIDValue();
      // Memtile DMAs can access neighboring tiles.
      if (targetModel.isMemTile(col, row)) {
        if (targetModel.isWest(col, row, lockCol, lockRow))
          lockID += 0;
        else if (targetModel.isInternal(col, row, lockCol, lockRow))
          lockID += targetModel.getNumLocks(lockCol, lockRow) * 1;
        else if (targetModel.isEast(col, row, lockCol, lockRow))
          lockID += targetModel.getNumLocks(lockCol, lockRow) * 2;
      }

      if (op.acquire() || op.acquireGE()) {
        hasAcq = true;
        acqLockID = lockID;
        acqValue = op.getLockValue();
        if (op.acquireGE())
          acqValue = -acqValue;
      } else if (op.release()) {
        hasRel = true;
        relLockID = lockID;
        relValue = op.getLockValue();
      } else {
        // unreachable for current targets
        return op.emitOpError("unsupported lock action");
      }
    }

    for (auto op : block.template getOps<DMABDPACKETOp>()) {
      foundBdPacket = true;
      packetType = op.getPacketType();
      packetID = op.getPacketID();
    }

    int bdNum = blockMap[&block];
    if (foundBd) {
      // TODO For now, we are going to name each dma desc with loc and bd
      // which we assume is unique. This is strictly not enforced but in
      // practice, this is true
      output << "XAie_DmaDesc " << tileDMAInstStr(col, row, bdNum) << ";\n";
      output << "__mlir_aie_try(XAie_DmaDescInit(" << deviceInstRef << ", "
             << tileDMAInstRefStr(col, row, bdNum) << ", "
             << tileLocStr(col, row) << "));\n";
      if (hasAcq || hasRel) {
        output << "__mlir_aie_try(XAie_DmaSetLock("
               << tileDMAInstRefStr(col, row, bdNum) << ", "
               << "XAie_LockInit(" << acqLockID << "," << acqValue << "),"
               << "XAie_LockInit(" << relLockID << "," << relValue << ")));\n";
        if (!hasAcq)
          output << tileDMAInstStr(col, row, bdNum)
                 << ".LockDesc.LockAcqEn = " << disable << ";\n";
        if (!hasRel)
          output << tileDMAInstStr(col, row, bdNum)
                 << ".LockDesc.LockRelEn = " << disable << ";\n";
      }

      if (0 == ndims) {
        if (targetModel.isShimNOCTile(col, row)) {
          output << "__mlir_aie_try(XAie_DmaSetAddrLen("
                 << tileDMAInstRefStr(col, row, bdNum) << ", /* addrA */ "
                 << "mlir_aie_external_get_addr_myBuffer_" << col << row << "_"
                 << bdNum << "(), "
                 << " /* len */ " << lenA << "));\n";
          output << "__mlir_aie_try(XAie_DmaSetAxi("
                 << tileDMAInstRefStr(col, row, bdNum) << ", "
                 << "/* smid */ 0, "
                 << "/* burstlen */ 4, "
                 << "/* QoS */ 0, "
                 << "/* Cache */ 0, "
                 << "/* Secure */ " << enable << "));\n";
        } else {
          if ((BaseAddrA + offsetA) % 4)
            return memOp.emitError("bd address must be 4B (32b) aligned");
          output << "__mlir_aie_try(XAie_DmaSetAddrLen("
                 << tileDMAInstRefStr(col, row, bdNum) << ", /* addrA */ "
                 << "0x" << llvm::utohexstr(BaseAddrA + offsetA) << ", "
                 << " /* len */ " << lenA << "));\n";
        }
      } else
        generateXAieDmaSetMultiDimAddr(output, ndims, dims, col, row, bdNum,
                                       BaseAddrA, offsetA, lenA,
                                       elementWidthInBytes, "1");

      if (block.getNumSuccessors() > 0) {
        Block *nextBlock = block.getSuccessors()[0]; // should have only one
                                                     // successor block

        int enableNextBd = 1;
        if (!nextBlock->getOps<EndOp>().empty())
          enableNextBd = 0;

        int nextBdNum = blockMap[nextBlock];
        output << "__mlir_aie_try(XAie_DmaSetNextBd("
               << tileDMAInstRefStr(col, row, bdNum) << ", "
               << " /* nextbd */ " << nextBdNum << ", "
               << " /* enableNextBd */ " << enableNextBd << "));\n";
      }

      if (foundBdPacket) {
        output << "__mlir_aie_try(XAie_DmaSetPkt("
               << tileDMAInstRefStr(col, row, bdNum) << ", "
               << packetStr(packetID, packetType) << "));\n";
      }
      output << "__mlir_aie_try(XAie_DmaEnableBd("
             << tileDMAInstRefStr(col, row, bdNum) << "));\n";
      output << "__mlir_aie_try(XAie_DmaWriteBd(" << deviceInstRef << ", "
             << tileDMAInstRefStr(col, row, bdNum) << ", "
             << tileLocStr(col, row) << ", "
             << " /* bd */ " << bdNum << "));\n";
    }
  }

  for (auto &block : memOp.getBody()) {
    for (auto op : block.template getOps<DMAStartOp>()) {
      int bdNum = blockMap[op.getDest()];
      StringRef dmaDir = stringifyDMAChannelDir(op.getChannelDir());
      int chNum = op.getChannelIndex();
      const auto &target_model = xilinx::AIE::getTargetModel(op);
      if (target_model.getTargetArch() == AIEArch::AIE1) {
        output << "__mlir_aie_try(XAie_DmaChannelPushBdToQueue("
               << deviceInstRef << ", " << tileLocStr(col, row) << ", "
               << "/* ChNum */" << chNum
               << ", "
               // TODO hack until physical dialect changes
               << "/* dmaDir */ DMA_" << dmaDir << ", "
               << "/* BdNum */" << bdNum << "));\n";
      } else {
        // in english repeat_count==0 means "do it once" and don't repeat but
        // libxaie treats repeat_count=1 as do it once.
        int repeatCount = op.getRepeatCount() + 1;
        output << "__mlir_aie_try(XAie_DmaChannelSetStartQueue("
               << deviceInstRef << ", " << tileLocStr(col, row) << ", "
               << "/* ChNum */" << chNum
               << ", "
               // TODO hack until physical dialect changes
               << "/* dmaDir */ DMA_" << dmaDir << ", "
               << "/* BdNum */" << bdNum << ", "
               << "/* Repeat */ " << repeatCount << ", "
               << "/* EnToken */ "
               << "XAIE_DISABLE"
               << "));\n";
      }
      output << "__mlir_aie_try(XAie_DmaChannelEnable(" << deviceInstRef << ", "
             << tileLocStr(col, row) << ", "
             << "/* ChNum */ " << chNum
             << ", "
             // TODO hack until physical dialect changes
             << "/* dmaDir */ DMA_" << dmaDir << "));\n";
    }
  }
  return success();
}

mlir::LogicalResult AIETranslateToXAIEV2(ModuleOp module, raw_ostream &output) {
  //  StringRef ctx   = "ctx";                     // TODO
  StringRef ctx_p = "aie_libxaie_ctx_t* ctx"; // TODO
  //  StringRef deviceInst = "ctx->DevInst";       // TODO
  StringRef deviceInstRef = "&(ctx->DevInst)"; // TODO

  DenseMap<TileID, Operation *> tiles;
  DenseMap<Operation *, SmallVector<BufferOp, 4>> buffers;

  if (module.getOps<DeviceOp>().empty())
    return module.emitOpError("expected AIE.device operation at toplevel");
  DeviceOp targetOp = *(module.getOps<DeviceOp>().begin());
  const auto &targetModel = targetOp.getTargetModel();

  collectTiles(targetOp, tiles);
  collectBuffers(targetOp, buffers);

  //---------------------------------------------------------------------------
  // mlir_aie_init_libxaie
  //---------------------------------------------------------------------------
  output << xaie_cpp_file_header;
  output << "aie_libxaie_ctx_t* mlir_aie_init_libxaie() {\n";
  output << "  aie_libxaie_ctx_t *ctx = new aie_libxaie_ctx_t;\n";
  output << "  if (!ctx)\n";
  output << "    return 0;\n";
  auto arch = targetModel.getTargetArch();
  std::string AIE1_device("XAIE_DEV_GEN_AIE");
  std::string AIE2_device("XAIE_DEV_GEN_AIEML");
  std::string AIE2p_device("XAIE_DEV_GEN_AIE2P");
  std::string device;
  switch (arch) {
  case AIEArch::AIE1:
    device = AIE1_device;
    break;
  case AIEArch::AIE2:
    device = AIE2_device;
    break;
  case AIEArch::AIE2p:
    device = AIE2p_device;
    break;
  }
  output << "  ctx->AieConfigPtr.AieGen = " << device << ";\n";
  output << "  ctx->AieConfigPtr.BaseAddr = 0x20000000000;\n";
  output << "  ctx->AieConfigPtr.ColShift = " << targetModel.getColumnShift()
         << ";\n";
  output << "  ctx->AieConfigPtr.RowShift = " << targetModel.getRowShift()
         << ";\n";
  output << "  ctx->AieConfigPtr.NumRows = " << targetModel.rows() << ";\n";
  output << "  ctx->AieConfigPtr.NumCols = " << targetModel.columns() << ";\n";
  output << "  ctx->AieConfigPtr.ShimRowNum = 0;\n";
  output << "  ctx->AieConfigPtr.MemTileRowStart = 1;\n";
  output << "  ctx->AieConfigPtr.MemTileNumRows = "
         << targetModel.getNumMemTileRows() << ";\n";
  output << "  //  ctx->AieConfigPtr.ReservedRowStart = "
            "XAIE_RES_TILE_ROW_START;\n";
  output
      << "  //  ctx->AieConfigPtr.ReservedNumRows  = XAIE_RES_TILE_NUM_ROWS;\n";
  output << "  ctx->AieConfigPtr.AieTileRowStart = "
         << (1 + targetModel.getNumMemTileRows()) << ";\n";
  output << "  ctx->AieConfigPtr.AieTileNumRows = "
         << (targetModel.rows() - 1 - targetModel.getNumMemTileRows()) << ";\n";
  output << "  ctx->AieConfigPtr.PartProp = {0};\n";
  output << "  ctx->DevInst = {0};\n";
  output << "  return ctx;\n";
  output << "}\n";
  output << "\n";

  //---------------------------------------------------------------------------
  // mlir_aie_configure_cores
  //---------------------------------------------------------------------------
  output << "int mlir_aie_configure_cores(" << ctx_p << ") {\n";
  // Reset each core.  Load the corresponding ELF file, if necessary.
  for (auto tileOp : targetOp.getOps<TileOp>()) {
    int col = tileOp.colIndex();
    int row = tileOp.rowIndex();
    if (tileOp.isShimTile() || tileOp.isMemTile()) {
      // Resets no needed with V2 kernel driver
    } else {
      // Resets no needed with V2 kernel driver
      output << "__mlir_aie_try(XAie_CoreReset(" << deviceInstRef << ", "
             << tileLocStr(col, row) << "));\n";
      output << "__mlir_aie_try(XAie_CoreDisable(" << deviceInstRef << ", "
             << tileLocStr(col, row) << "));\n";
      // Release locks
      int numLocks = targetModel.getNumLocks(col, row);
      output << "for (int l = 0; l < " << numLocks << "; ++l)\n"
             << "  __mlir_aie_try(XAie_LockRelease(" << deviceInstRef << ", "
             << tileLocStr(col, row) << ", XAie_LockInit(l, 0x0), 0));\n";
      if (auto coreOp = tileOp.getCoreOp()) {
        std::string fileName;
        if (auto fileAttr = coreOp.getElfFile())
          fileName = fileAttr.value().str();
        else
          fileName = std::string("core_") + std::to_string(col) + "_" +
                     std::to_string(row) + ".elf";
        output << "{\n"
               << "AieRC RC = XAie_LoadElf(" << deviceInstRef << ", "
               << tileLocStr(col, row) << ", "
               << "(const char*)\"" << fileName << "\",0);\n";
        output << "if (RC != XAIE_OK)\n"
               << "    __mlir_aie_verbose(fprintf(stderr, \"Failed to load elf "
                  "for Core[%d,%d], ret is %d\\n\", "
               << std::to_string(col) << ", " << std::to_string(row)
               << ", RC));\n"
               << "assert(RC == XAIE_OK);\n"
               << "}\n";
      }
    }
  }
  output << "return XAIE_OK;\n";
  output << "} // mlir_aie_configure_cores\n\n";

  //---------------------------------------------------------------------------
  // mlir_aie_start_cores
  //---------------------------------------------------------------------------
  output << "int mlir_aie_start_cores(" << ctx_p << ") {\n";
  // Start execution of all the cores.
  for (auto tileOp : targetOp.getOps<TileOp>()) {
    int col = tileOp.colIndex();
    int row = tileOp.rowIndex();
    if (!tileOp.isShimTile() && !tileOp.isMemTile()) {
      output << "__mlir_aie_try(XAie_CoreUnreset(" << deviceInstRef << ", "
             << tileLocStr(col, row) << "));\n";
      output << "__mlir_aie_try(XAie_CoreEnable(" << deviceInstRef << ", "
             << tileLocStr(col, row) << "));\n";
    }
  }
  output << "return XAIE_OK;\n";
  output << "} // mlir_aie_start_cores\n\n";

  //---------------------------------------------------------------------------
  // mlir_aie_configure_dmas
  //---------------------------------------------------------------------------
  output << "int mlir_aie_configure_dmas(" << ctx_p << ") {\n";

  // DMA configuration
  // AieRC XAie_DmaDescInit(XAie_DevInst *DevInst, XAie_DmaDesc *DmaDesc,
  // XAie_LocType Loc); AieRC XAie_DmaSetLock(XAie_DmaDesc *DmaDesc, XAie_Lock
  // Acq, XAie_Lock Rel); AieRC XAie_DmaSetPkt(XAie_DmaDesc *DmaDesc,
  // XAie_Packet Pkt); AieRC XAie_DmaSetOutofOrderBdId(XAie_DmaDesc *DmaDesc, u8
  // OutofOrderBdId); AieRC XAie_DmaSetDoubleBuffer(XAie_DmaDesc *DmaDesc, u64
  // Addr, XAie_Lock Acq, XAie_Lock Rel); AieRC XAie_DmaSetAddrLen(XAie_DmaDesc
  // *DmaDesc, u64 Addr, u32 Len); AieRC XAie_DmaSetMultiDimAddr(XAie_DmaDesc
  // *DmaDesc, XAie_DmaTensor *Tensor, u64 Addr, u32 Len); AieRC
  // XAie_DmaEnableCompression(XAie_DmaDesc *DmaDesc); AieRC
  // XAie_DmaSetNextBd(XAie_DmaDesc *DmaDesc, u8 NextBd, u8 EnableNextBd); AieRC
  // XAie_DmaEnableBd(XAie_DmaDesc *DmaDesc); AieRC
  // XAie_DmaDisableBd(XAie_DmaDesc *DmaDesc); AieRC XAie_DmaSetAxi(XAie_DmaDesc
  // *DmaDesc, u8 Smid, u8 BurstLen, u8 Qos,u8 Cache, u8 Secure); AieRC
  // XAie_DmaSetInterleaveEnable(XAie_DmaDesc *DmaDesc, u8 DoubleBuff, u8
  // IntrleaveCount, u16 IntrleaveCurr); AieRC XAie_DmaWriteBd(XAie_DevInst
  // *DevInst, XAie_DmaDesc *DmaDesc, XAie_LocType Loc, u8 BdNum);

  // AieRC XAie_DmaChannelResetAll(XAie_DevInst *DevInst, XAie_LocType Loc,
  // XAie_DmaChReset Reset); AieRC XAie_DmaChannelReset(XAie_DevInst *DevInst,
  // XAie_LocType Loc, u8 ChNum, XAie_DmaDirection Dir, XAie_DmaChReset Reset);
  // AieRC XAie_DmaChannelPauseStream(XAie_DevInst *DevInst, XAie_LocType Loc,
  // u8 ChNum, XAie_DmaDirection Dir, u8 Pause); AieRC
  // XAie_DmaChannelPauseMem(XAie_DevInst *DevInst, XAie_LocType Loc, u8 ChNum
  // XAie_DmaDirection Dir, u8 Pause); AieRC XAie_DmaChannelConfig(XAie_DevInst
  // *DevInst, XAie_DmaDesc *DmaDesc, XAie_LocType Loc, u8 ChNum,
  // XAie_DmaDirection Dir, u8 RepeatCount, u8 EnTokenIssue, u8 ControllerId);
  // AieRC XAie_DmaChannelPushBdToQueue(XAie_DevInst *DevInst, XAie_LocType Loc,
  // u8 ChNum, XAie_DmaDirection Dir, u8 BdNum); AieRC
  // XAie_DmaChannelEnable(XAie_DevInst *DevInst, XAie_LocType Loc, u8 ChNum,
  // XAie_DmaDirection Dir); AieRC XAie_DmaChannelDisable(XAie_DevInst *DevInst,
  // XAie_LocType Loc, u8 ChNum, XAie_DmaDirection Dir);
  for (auto memOp : targetOp.getOps<MemOp>()) {
    DenseMap<Block *, int> blockMap;

    // Assign each block a BD number
    int bdNum = 0;
    for (auto &block : memOp.getBody()) {
      if (!block.getOps<DMABDOp>().empty()) {
        blockMap[&block] = bdNum;
        bdNum++;
      }
    }
    auto result = generateDMAConfig(memOp, output, targetModel, blockMap);
    if (result.failed())
      return result;
  }
  for (auto memOp : targetOp.getOps<MemTileDMAOp>()) {
    DenseMap<Block *, int> blockMap;
    // Memtiles have restrictions on which channels can access which BDs
    DenseMap<Block *, int> channelMap;

    for (auto &block : memOp.getBody()) {
      for (auto op : block.getOps<DMAStartOp>()) {
        int chNum = op.getChannelIndex();
        channelMap[&block] = chNum;
        auto dest = op.getDest();
        while (dest) {
          channelMap[dest] = chNum;
          if (dest->getSuccessors().size() < 1)
            break;
          dest = dest->getSuccessors()[0];
          if (channelMap.count(dest))
            break;
        }
      }
    }

    // Assign each block a BD number
    int evenBdNum = 0;
    int oddBdNum = 24;
    for (auto &block : memOp.getBody()) {
      if (block.getOps<DMABDOp>().empty())
        continue;
      assert(channelMap.count(&block));
      if (channelMap[&block] & 1)
        blockMap[&block] = oddBdNum++;
      else
        blockMap[&block] = evenBdNum++;
    }
    auto result = generateDMAConfig(memOp, output, targetModel, blockMap);
    if (result.failed())
      return result;
  }

  output << "return XAIE_OK;\n";
  output << "} // mlir_aie_configure_dmas\n\n";

  for (auto op : targetOp.getOps<ExternalBufferOp>()) {
    if (op.hasName()) {
      output << "static u64 _mlir_aie_external_" << op.name().getValue()
             << ";\n";
      output << "static bool _mlir_aie_external_set_" << op.name().getValue()
             << " = false;\n";

      output << "void mlir_aie_external_set_addr_" << op.name().getValue()
             << "(" << ctx_p << ", u64 VA) {\n"
             << "  u64 device_address = mlir_aie_get_device_address(ctx, (void "
                "*)VA);\n"
             << "    _mlir_aie_external_set_" << op.name().getValue()
             << " = true;\n"
             << "    _mlir_aie_external_" << op.name().getValue()
             << " = device_address;\n"
             << "}\n";
    }
  }

  // ShimDMA Config
  //  int index = 0;
  for (auto op : targetOp.getOps<ShimDMAOp>()) {
    int col = op.colIndex();
    int row = op.rowIndex();

    DenseMap<Block *, int> blockMap;
    {
      // Assign each block a BD number
      int bdNum = 0;
      for (auto &block : op.getBody()) {
        if (!block.getOps<DMABDOp>().empty()) {
          blockMap[&block] = bdNum;
          uint64_t offset = 0;
          for (auto op : block.getOps<DMABDOp>()) {
            offset = op.getOffsetInBytes();
            auto buffer =
                cast<ExternalBufferOp>(op.getBuffer().getDefiningOp());

            output << "u64 mlir_aie_external_get_addr_myBuffer_" << col << row
                   << "_" << bdNum << "(void) {\n"
                   << "    assert(_mlir_aie_external_set_"
                   << buffer.name().getValue() << ");\n"
                   << "    return _mlir_aie_external_"
                   << buffer.name().getValue() << " + "
                   << llvm::utohexstr(offset) << ";\n"
                   << "}\n";
          }

          bdNum++;
        }
      }
    }

    output << "int mlir_aie_configure_shimdma_" << col << row << "(" << ctx_p
           << ") {\n";
    auto result = generateDMAConfig(op, output, targetModel, blockMap);
    if (result.failed())
      return result;
    output << "return XAIE_OK;\n";
    output << "} // mlir_aie_configure_shimdma\n\n";
  }

  //---------------------------------------------------------------------------
  // mlir_aie_initialize_locks
  //---------------------------------------------------------------------------
  output << "int mlir_aie_initialize_locks(" << ctx_p << ") {\n";
  // Lock configuration
  targetOp.walk<WalkOrder::PreOrder>([&](LockOp lock) {
    TileOp tile = lock.getTileOp();
    int col = tile.colIndex();
    int row = tile.rowIndex();
    int lockID = lock.getLockIDValue();
    auto init = lock.getInit();
    if (init)
      output << "__mlir_aie_try(XAie_LockSetValue(" << deviceInstRef << ", "
             << tileLocStr(col, row) << ", "
             << "XAie_LockInit(" << lockID << ", " << *init << ")));\n";
  });
  output << "return XAIE_OK;\n";
  output << "} // mlir_aie_initialize_locks\n";

  //---------------------------------------------------------------------------
  // mlir_aie_configure_switchboxes
  //---------------------------------------------------------------------------
  output << "int mlir_aie_configure_switchboxes(" << ctx_p << ") {\n";
  output << "  int x, y;\n";

  // StreamSwitch (switchbox) configuration
  for (auto switchboxOp : targetOp.getOps<SwitchboxOp>()) {
    Region &r = switchboxOp.getConnections();
    Block &b = r.front();
    bool isEmpty = b.getOps<ConnectOp>().empty() &&
                   b.getOps<MasterSetOp>().empty() &&
                   b.getOps<PacketRulesOp>().empty();
    bool isParam = false;

    if (isa<TileOp>(switchboxOp.getTile().getDefiningOp())) {
      int col = switchboxOp.colIndex();
      int row = switchboxOp.rowIndex();
      if (!isEmpty) {
        output << "// Core Stream Switch column " << col << " row " << row
               << "\n";
        output << "x = " << col << ";\n";
        output << "y = " << row << ";\n";
      }
    } else if (auto sel =
                   dyn_cast<SelectOp>(switchboxOp.getTile().getDefiningOp())) {
      // parameterize streamswitch's configuration
      isParam = true;
      HerdOp sourceHerd = cast<HerdOp>(sel.getStartHerd().getDefiningOp());
      std::string sourceHerdName(sourceHerd.name().getValue());

      IterOp iterX = cast<IterOp>(sel.getIterX().getDefiningOp());
      IterOp iterY = cast<IterOp>(sel.getIterY().getDefiningOp());
      int startXValue = iterX.getStartValue();
      int endXValue = iterX.getEndValue();
      int strideXValue = iterX.getStrideValue();
      int startYValue = iterY.getStartValue();
      int endYValue = iterY.getEndValue();
      int strideYValue = iterY.getStrideValue();

      std::string startX(sourceHerdName + "_X + " +
                         std::to_string(startXValue));
      std::string endX(sourceHerdName + "_X + " + std::to_string(endXValue));
      std::string startY(sourceHerdName + "_Y + " +
                         std::to_string(startYValue));
      std::string endY(sourceHerdName + "_Y + " + std::to_string(endYValue));

      output << "for (x = " << startX << "; x < " << endX
             << "; x += " << strideXValue << ") {\n";
      output << "for (y = " << startY << "; y < " << endY
             << "; y += " << strideYValue << ") {\n";
    }

    for (auto connectOp : b.getOps<ConnectOp>())
      output << "__mlir_aie_try(XAie_StrmConnCctEnable(" << deviceInstRef
             << ", " << tileLocStr("x", "y") << ", "
             << stringifyWireBundle(connectOp.getSourceBundle()).upper() << ", "
             << connectOp.sourceIndex() << ", "
             << stringifyWireBundle(connectOp.getDestBundle()).upper() << ", "
             << connectOp.destIndex() << "));\n";

    for (auto connectOp : b.getOps<MasterSetOp>()) {
      int mask = 0;
      int arbiter = -1;
      for (auto val : connectOp.getAmsels()) {
        AMSelOp amsel = cast<AMSelOp>(val.getDefiningOp());
        arbiter = amsel.arbiterIndex();
        int msel = amsel.getMselValue();
        mask |= (1 << msel);
      }
      bool isdma = (connectOp.getDestBundle() == WireBundle::DMA);

      output << "__mlir_aie_try(XAie_StrmPktSwMstrPortEnable(" << deviceInstRef
             << ", " << tileLocStr("x", "y") << ", "
             << stringifyWireBundle(connectOp.getDestBundle()).upper() << ", "
             << connectOp.destIndex() << ", "
             << "/* drop_header */ "
             << (isdma ? "XAIE_SS_PKT_DROP_HEADER"
                       : "XAIE_SS_PKT_DONOT_DROP_HEADER")
             << ", "
             << "/* arbiter */ " << arbiter << ", "
             << "/* MSelEn */ "
             << "0x" << llvm::utohexstr(mask) << "));\n";
    }

    for (auto connectOp : b.getOps<PacketRulesOp>()) {
      int slot = 0;
      Block &block = connectOp.getRules().front();
      for (auto slotOp : block.getOps<PacketRuleOp>()) {
        AMSelOp amselOp = cast<AMSelOp>(slotOp.getAmsel().getDefiningOp());
        int arbiter = amselOp.arbiterIndex();
        int msel = amselOp.getMselValue();
        output << "__mlir_aie_try(XAie_StrmPktSwSlavePortEnable("
               << deviceInstRef << ", " << tileLocStr("x", "y") << ", "
               << stringifyWireBundle(connectOp.getSourceBundle()).upper()
               << ", " << connectOp.sourceIndex() << "));\n";

        // TODO Need to better define packet id,type used here
        output << "__mlir_aie_try(XAie_StrmPktSwSlaveSlotEnable("
               << deviceInstRef << ", " << tileLocStr("x", "y") << ", "
               << stringifyWireBundle(connectOp.getSourceBundle()).upper()
               << ", " << connectOp.sourceIndex() << ", "
               << "/* slot */ " << slot << ", "
               << "/* packet */ " << packetStr(slotOp.valueInt(), /*type*/ 0)
               << ", "
               << "/* mask */ "
               << "0x" << llvm::utohexstr(slotOp.maskInt()) << ", "
               << "/* msel */ " << msel << ", "
               << "/* arbiter */ " << arbiter << "));\n";
        slot++;
      }
    }

    if (isParam) {
      output << "}\n";
      output << "}\n";
    }
  }
  for (auto op : targetOp.getOps<ShimMuxOp>()) {
    Region &r = op.getConnections();
    Block &b = r.front();
    bool isEmpty = b.getOps<ConnectOp>().empty();

    if (isa<TileOp>(op.getTile().getDefiningOp())) {
      int col = op.colIndex();
      int row = op.rowIndex();
      if (!isEmpty) {
        output << "// ShimMux column " << col << " row " << row << "\n";
        output << "// NOTE ShimMux always connects from the south as "
               << "directions are defined relative to the tile stream "
               << "switch\n";
        output << "x = " << col << ";\n";
        output << "y = " << row << ";\n";
      }
    }

    for (auto connectOp : b.getOps<ConnectOp>()) {

      if (connectOp.getSourceBundle() == WireBundle::DMA ||
          connectOp.getDestBundle() == WireBundle::DMA) {
        if (connectOp.getSourceBundle() == WireBundle::North)
          // demux!
          output
              << "__mlir_aie_try(XAie_EnableAieToShimDmaStrmPort("
              << deviceInstRef << ", " << tileLocStr("x", "y")
              << ", "
              //               <<
              //               stringifyWireBundle(connectOp.sourceBundle()).upper()
              << connectOp.sourceIndex() << "));\n";
        else if (connectOp.getDestBundle() == WireBundle::North)
          // mux
          output
              << "__mlir_aie_try(XAie_EnableShimDmaToAieStrmPort("
              << deviceInstRef << ", " << tileLocStr("x", "y")
              << ", "
              //               <<
              //               stringifyWireBundle(connectOp.sourceBundle()).upper()
              << connectOp.destIndex() << "));\n";
      }

      else if (connectOp.getSourceBundle() == WireBundle::PLIO ||
               connectOp.getDestBundle() == WireBundle::PLIO) {
        if (connectOp.getSourceBundle() == WireBundle::North) {
          // mux
          output << "__mlir_aie_try(XAie_AieToPlIntfEnable(" << deviceInstRef
                 << ", " << tileLocStr("x", "y") << ", "
                 << connectOp.destIndex() << ", PLIF_WIDTH_64));\n";
        } else if (connectOp.getDestBundle() == WireBundle::North) {
          // mux
          output << "__mlir_aie_try(XAie_PlToAieIntfEnable(" << deviceInstRef
                 << ", " << tileLocStr("x", "y") << ", "
                 << connectOp.destIndex() << ", PLIF_WIDTH_64));\n";
        }
      }
    }
  }
  for (auto switchboxOp : targetOp.getOps<ShimSwitchboxOp>()) {
    Region &r = switchboxOp.getConnections();
    Block &b = r.front();
    bool isEmpty = b.getOps<ConnectOp>().empty();
    int col = switchboxOp.getCol();
    if (!isEmpty)
      output << "// Shim Switch column " << col << "\n";
    for (auto connectOp : b.getOps<ConnectOp>())
      output << "__mlir_aie_try(XAie_StrmConnCctEnable(" << deviceInstRef
             << ", " << tileLocStr(col, 0) << ", "
             << stringifyWireBundle(connectOp.getSourceBundle()).upper() << ", "
             << connectOp.sourceIndex() << ", "
             << stringifyWireBundle(connectOp.getDestBundle()).upper() << ", "
             << connectOp.destIndex() << "));\n";
  }

  output << "return XAIE_OK;\n";
  output << "} // mlir_aie_configure_switchboxes\n\n";

  //---------------------------------------------------------------------------
  // mlir_aie_configure_cascade
  //---------------------------------------------------------------------------
  output << "int mlir_aie_configure_cascade(" << ctx_p << ") {\n";
  for (auto configOp : targetOp.getOps<ConfigureCascadeOp>()) {
    TileOp tile = cast<TileOp>(configOp.getTile().getDefiningOp());
    int col = tile.colIndex();
    int row = tile.rowIndex();
    output << "XAie_CoreConfigAccumulatorControl(" << deviceInstRef << ", "
           << "XAie_TileLoc(" << col << ", " << row << "), "
           << stringifyCascadeDir(configOp.getInputDir()).upper() << ", "
           << stringifyCascadeDir(configOp.getOutputDir()).upper() << ");\n";
  }
  output << "return XAIE_OK;\n";
  output << "} // mlir_aie_configure_cascade\n\n";

  //---------------------------------------------------------------------------
  // Output Buffer Accessors
  //---------------------------------------------------------------------------
  for (auto tile : tiles) {
    Operation *tileOp = tile.second;
    TileID coord = cast<TileOp>(tileOp).getTileID();
    int col = coord.col;
    int row = coord.row;
    auto loc = tileLocStr(col, row);

    auto bufferAccessor = [&](BufferOp buf) {
      // int32_t mlir_aie_read_buffer_a13(int index) {
      // void mlir_aie_write_buffer_a13(int index, int32_t value) {
      std::string bufName(buf.name().getValue());
      Type t = buf.getType();
      Type et;
      std::string typestr;
      if (auto memrefType = llvm::dyn_cast<MemRefType>(t)) {
        et = memrefType.getElementType();
        if (et.isInteger(32))
          typestr = "int32_t";
        else if (et.isF32())
          typestr = "float";
        else {
          output << "// buffer " << bufName << " with unsupported type " << t
                 << ";\n";
          return; // Unsupported type
        }

      } else {
        output << "// buffer " << bufName << " with unsupported type " << t
               << ";\n";
        return; // Unsupported type
      }
      assert(buf.getAddress().has_value() && "buffer must have address");
      output << "const int " << bufName
             << "_offset = " << buf.getAddress().value() << ";\n";
      output << typestr << " mlir_aie_read_buffer_" << bufName << "(" << ctx_p
             << ", int index) {\n";
      output << "u32 value; auto rc = XAie_DataMemRdWord(" << deviceInstRef
             << ", " << loc << ", " << bufName
             << "_offset + (index*4), &value);\n";
      if (et.isInteger(32))
        output << "  return value;\n";
      else if (et.isF32()) {
        output << "  union caster { int32_t i; float f; };\n";
        output << "  caster c; c.i = value;\n";
        output << "  return c.f;\n";
      }
      output << "}\n";
      output << "int mlir_aie_write_buffer_" << bufName << "(" << ctx_p
             << ", int index, " << typestr << " value) {\n";
      if (et.isInteger(32))
        output << "  int32_t int_value = value;\n";
      else if (et.isF32()) {
        output << "  union caster { int32_t i; float f; };\n";
        output << "  caster c; c.f = value;\n";
        output << "  int32_t int_value = c.i;\n";
      }
      output << "AieRC rc =    XAie_DataMemWrWord(" << deviceInstRef << ", "
             << loc << ", " << bufName << "_offset + (index*4), int_value);\n";
      output << "return rc;\n";
      output << "}\n";
    };

    // if(tiles.count(tile.getValue()))
    for (auto buf : buffers[tileOp])
      bufferAccessor(buf);
  }

  auto lockAccessor = [&](LockOp lock) {
    int col = lock.colIndex();
    int row = lock.rowIndex();
    if (!lock.hasName())
      return;
    std::string lockName(lock.name().getValue());
    output << "int mlir_aie_acquire_" << lockName << "(" << ctx_p
           << ", int value, int timeout) {\n";
    output << "  const int id = " << lock.getLockIDValue() << ";\n";
    output << "  return XAie_LockAcquire(" << deviceInstRef << ", "
           << tileLocStr(col, row) << ", " << tileLockStr("id", "value")
           << ", timeout);\n";
    output << "}\n";
    output << "int mlir_aie_release_" << lockName << "(" << ctx_p
           << ", int value, int timeout) {\n";
    output << "  const int id = " << lock.getLockIDValue() << ";\n";
    output << "  return XAie_LockRelease(" << deviceInstRef << ", "
           << tileLocStr(col, row) << ", " << tileLockStr("id", "value")
           << ", timeout);\n";
    output << "}\n";
  };

  targetOp.walk<WalkOrder::PreOrder>([&](LockOp lock) { lockAccessor(lock); });

  return success();
}
} // namespace xilinx::AIE
