/*
 Nachtmaschine — FreeUSD-based entity model previews for USD stages (GPL-3 stack).
*/

#include "mdl/LoadFreeUsdModel.h"

#include "Logger.h"
#include "Result.h"
#include "fs/FileSystem.h"
#include "gl/IndexRangeMapBuilder.h"
#include "gl/TextureResource.h"
#include "mdl/EntityModel.h"
#include "mdl/MaterialUtils.h"

#include "kd/path_utils.h"
#include "kd/result.h"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#ifndef TB_HAS_FREEUSD
#define TB_HAS_FREEUSD 0
#endif

#if TB_HAS_FREEUSD

#include "freeusd/gf/bbox3d.hpp"
#include "freeusd/usd/stage.hpp"
#include "freeusd/usdUtils/engineScene.hpp"

#include "vm/bbox.h"
#include "vm/vec.h"

namespace tb::mdl
{
namespace
{

struct TempUsdFile
{
  std::filesystem::path path;
  ~TempUsdFile()
  {
    std::error_code ec;
    if (!path.empty())
    {
      std::filesystem::remove(path, ec);
    }
  }
};

vm::vec3f toVec3f(const freeusd::gf::Vec3d& v)
{
  return vm::vec3f{float(v.x()), float(v.y()), float(v.z())};
}

vm::bbox3f toBBox(const freeusd::gf::BBox3d& b)
{
  if (b.IsEmpty())
  {
    const auto h = vm::vec3f::fill(0.5f);
    return vm::bbox3f{-h, h};
  }
  return vm::bbox3f{toVec3f(b.min), toVec3f(b.max)};
}

vm::bbox3f expandIfDegenerate(vm::bbox3f box)
{
  const auto e = vm::vec3f::fill(0.01f);
  if (box.min == box.max)
  {
    return vm::bbox3f{box.min - e, box.max + e};
  }
  return box;
}

void addAxisAlignedBox(gl::IndexRangeMapBuilder<EntityModelVertex::Type>& builder, const vm::bbox3f& b)
{
  const auto& mn = b.min;
  const auto& mx = b.max;
  const auto uv = vm::vec2f{0.0f, 0.0f};

  const auto v = [&](const float x, const float y, const float z) {
    return EntityModelVertex{vm::vec3f{x, y, z}, uv};
  };

  // Six faces, two triangles each (outward winding for typical +Z-up scenes).
  const float x0 = mn.x(), y0 = mn.y(), z0 = mn.z();
  const float x1 = mx.x(), y1 = mx.y(), z1 = mx.z();

  // -Z
  builder.addTriangle(v(x0, y0, z0), v(x1, y0, z0), v(x1, y1, z0));
  builder.addTriangle(v(x0, y0, z0), v(x1, y1, z0), v(x0, y1, z0));
  // +Z
  builder.addTriangle(v(x0, y0, z1), v(x0, y1, z1), v(x1, y1, z1));
  builder.addTriangle(v(x0, y0, z1), v(x1, y1, z1), v(x1, y0, z1));
  // -X
  builder.addTriangle(v(x0, y0, z0), v(x0, y1, z0), v(x0, y1, z1));
  builder.addTriangle(v(x0, y0, z0), v(x0, y1, z1), v(x0, y0, z1));
  // +X
  builder.addTriangle(v(x1, y0, z0), v(x1, y0, z1), v(x1, y1, z1));
  builder.addTriangle(v(x1, y0, z0), v(x1, y1, z1), v(x1, y1, z0));
  // -Y
  builder.addTriangle(v(x0, y0, z0), v(x0, y0, z1), v(x1, y0, z1));
  builder.addTriangle(v(x0, y0, z0), v(x1, y0, z1), v(x1, y0, z0));
  // +Y
  builder.addTriangle(v(x0, y1, z0), v(x1, y1, z0), v(x1, y1, z1));
  builder.addTriangle(v(x0, y1, z0), v(x1, y1, z1), v(x0, y1, z1));
}

} // namespace

bool canLoadFreeUsdModel(const std::filesystem::path& path)
{
  static const auto ext = kdl::path_to_lower(path.extension());
  return ext == ".usd" || ext == ".usda" || ext == ".usdc";
}

Result<EntityModelData> loadFreeUsdModel(
  const std::filesystem::path& path, const fs::FileSystem& fs, Logger& logger)
{
  return fs.openFile(path) | kdl::and_then([&](auto file) -> Result<EntityModelData> {
           auto reader = file->reader().buffer();
           const auto view = reader.stringView();

           std::random_device rd;
           const auto tmpName = fmt::format(
             "tb_entity_usd_{}_{}{}",
             rd(),
             std::hash<std::string>{}(path.generic_string()),
             path.extension().u8string());
           TempUsdFile tmp{std::filesystem::temp_directory_path() / tmpName};
           {
             std::ofstream out(tmp.path, std::ios::binary);
             if (!out)
             {
               return Error{"Could not create temporary file for USD stage"};
             }
             out.write(view.data(), static_cast<std::streamsize>(view.size()));
             if (!out)
             {
               return Error{"Could not write temporary USD bytes"};
             }
           }

           std::string err;
           auto stage = freeusd::usd::Stage::OpenFromRootFile(
             tmp.path.string(), freeusd::usd::RootLayerSublayersPolicy::DepthFirst, &err);
           if (!stage)
           {
             logger.warn() << fmt::format("FreeUSD could not open '{}': {}", path.generic_string(), err);
             return Error{fmt::format("FreeUSD could not open stage: {}", err)};
           }

           const auto snapshot = freeusd::usdUtils::BuildEngineSceneSnapshot(*stage, 1.0);

           auto boundsAcc = vm::bbox3f::builder{};
           auto meshBuilder = gl::IndexRangeMapBuilder<EntityModelVertex::Type>{};

           for (const auto& node : snapshot.nodes)
           {
             if (node.path == snapshot.pseudo_root_path)
             {
               continue;
             }
             if (!node.visible)
             {
               continue;
             }
             if (node.world_bound.IsEmpty())
             {
               continue;
             }

             auto box = expandIfDegenerate(toBBox(node.world_bound));
             boundsAcc.add(box);
             addAxisAlignedBox(meshBuilder, box);
           }

           if (!boundsAcc.initialized())
           {
             addAxisAlignedBox(meshBuilder, vm::bbox3f{-vm::vec3f::fill(0.5f), vm::vec3f::fill(0.5f)});
             boundsAcc.add(vm::bbox3f{-vm::vec3f::fill(0.5f), vm::vec3f::fill(0.5f)});
           }

           auto data = EntityModelData{PitchType::Normal, Orientation::Oriented};
           auto& frame = data.addFrame("usd", boundsAcc.bounds());
           auto& surface = data.addSurface("usd_preview", 1);
           auto defaultTex = loadDefaultTexture(fs, logger);
           surface.setSkins(std::vector<gl::Material>{
             gl::Material{"", gl::createTextureResource(std::move(defaultTex))}});
           surface.addMesh(frame, meshBuilder.vertices(), meshBuilder.indices());
           return std::move(data);
         });
}

} // namespace tb::mdl

#else // !TB_HAS_FREEUSD

namespace tb::mdl
{

bool canLoadFreeUsdModel(const std::filesystem::path& path)
{
  (void)path;
  return false;
}

Result<EntityModelData> loadFreeUsdModel(
  const std::filesystem::path& path, const fs::FileSystem& fs, Logger& logger)
{
  (void)path;
  (void)fs;
  (void)logger;
  return Error{"Editor was built without FreeUSD (CMake option NACHT_EDITOR_WITH_FREEUSD)."};
}

} // namespace tb::mdl

#endif
