// Copyright 2025 QuantClaw Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "quantclaw/skill/skill_loader_full.hpp"
#include "quantclaw/skill/skill_parse_file.hpp"

namespace quantclaw {

// ---------------------------------------------------------------------------
// SkillInstallDeps: skill dependency installation.
//
// Responsibilities:
//   Installs skill dependencies via npm, go, uv, apt, brew, or download.
//   Supports checking binary availability before install.
//
// Usage:
//   SkillLoaderFull full_loader(logger);
//   SkillInstallDeps installer(full_loader, logger);
//
//   // Install a single skill's dependencies
//   installer.InstallSkillDependency(skill_file_data);
//
//   // Install by skill name
//   installer.InstallSkillDependencyByName("weather");
//
//   // Install all skills with dependencies
//   installer.InstallAllSkillDependencies(metas);
// ---------------------------------------------------------------------------

class SkillInstallDeps {
 public:
  // Construct with reference to the full loader.
  SkillInstallDeps(std::shared_ptr<spdlog::logger> logger);

  // -----------------------------------------------------------------------
  // Skill dependency installation
  // -----------------------------------------------------------------------

  // Install a skill's dependencies. Returns true if all install steps succeed.
  // Supports node (npm), go, uv, apt, brew, and download methods.
  bool InstallSkillDependence(const SkillFullData& skill);

  // Install a skill's dependencies by name. Searches the metadata list
  // for the skill, loads its full data, and runs the install.
  // Returns true on success, false if skill not found or install fails.
  bool InstallSkillDependenceByName(
      const std::string& skill_name,
      const std::vector<SkillMetadata>& metas);

  // Install all skills that have install instructions and whose gating passes.
  // Returns the number of skills successfully installed.
  int InstallAllSkillDependences(const std::vector<SkillMetadata>& metas);

 private:
  // Check if binary exists in PATH
  bool IsBinaryAvailable(const std::string& binary_name) const;

  // Reference to the full loader
  std::shared_ptr<SkillLoaderFull> full_loader_;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace quantclaw
