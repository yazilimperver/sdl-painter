#include "shader_program.h"

#include <fstream>
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <vector>

namespace sdl_painter {

ShaderProgram::~ShaderProgram() {
  if (mProgramId != 0) {
    glDeleteProgram(mProgramId);
    mProgramId = 0;
  }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : mProgramId(other.mProgramId),
      mUniformCache(std::move(other.mUniformCache)) {
  other.mProgramId = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
  if (this != &other) {
    if (mProgramId != 0) {
      glDeleteProgram(mProgramId);
    }
    mProgramId = other.mProgramId;
    mUniformCache = std::move(other.mUniformCache);
    other.mProgramId = 0;
  }
  return *this;
}

uint32_t ShaderProgram::CompileShader(uint32_t type, const std::string& src) {
  uint32_t shader = glCreateShader(type);
  const char* src_ptr = src.c_str();
  glShaderSource(shader, 1, &src_ptr, nullptr);
  glCompileShader(shader);

  int32_t success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success == 0) {
    int32_t log_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
    // log_len bazı sürücülerde 0 dönebilir; en az 1 byte boş string için tahsis et.
    std::vector<char> log(static_cast<std::size_t>(log_len > 0 ? log_len : 1));
    if (log_len > 0) {
      glGetShaderInfoLog(shader, log_len, nullptr, log.data());
    } else {
      log[0] = '\0';
    }
    spdlog::error("[ShaderProgram] Compile error:\n{}", log.data());
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

bool ShaderProgram::Build(const std::string& vert_src,
                          const std::string& frag_src) {
  uint32_t vert = CompileShader(GL_VERTEX_SHADER, vert_src);
  if (vert == 0) {
    return false;
  }

  uint32_t frag = CompileShader(GL_FRAGMENT_SHADER, frag_src);
  if (frag == 0) {
    glDeleteShader(vert);
    return false;
  }

  mProgramId = glCreateProgram();
  glAttachShader(mProgramId, vert);
  glAttachShader(mProgramId, frag);
  glLinkProgram(mProgramId);
  glDeleteShader(vert);
  glDeleteShader(frag);

  int32_t success = 0;
  glGetProgramiv(mProgramId, GL_LINK_STATUS, &success);
  if (success == 0) {
    int32_t log_len = 0;
    glGetProgramiv(mProgramId, GL_INFO_LOG_LENGTH, &log_len);
    std::vector<char> log(static_cast<std::size_t>(log_len > 0 ? log_len : 1));
    if (log_len > 0) {
      glGetProgramInfoLog(mProgramId, log_len, nullptr, log.data());
    } else {
      log[0] = '\0';
    }
    spdlog::error("[ShaderProgram] Link error:\n{}", log.data());
    glDeleteProgram(mProgramId);
    mProgramId = 0;
    return false;
  }
  return true;
}

bool ShaderProgram::BuildFromFile(const std::string& vert_path,
                                  const std::string& frag_path) {
  auto read_file = [](const std::string& path) -> std::string {
    std::ifstream file(path);
    if (!file.is_open()) {
      spdlog::error("[ShaderProgram] Failed to open file: {}", path);
      return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
  };

  std::string vert_src = read_file(vert_path);
  std::string frag_src = read_file(frag_path);

  if (vert_src.empty() || frag_src.empty()) {
    return false;
  }

  return Build(vert_src, frag_src);
}

void ShaderProgram::Use() const {
  glUseProgram(mProgramId);
}

int32_t ShaderProgram::GetUniformLocation(const std::string& name) const {
  auto it = mUniformCache.find(name);
  if (it != mUniformCache.end()) {
    return it->second;
  }
  const int32_t kLoc = glGetUniformLocation(mProgramId, name.c_str());
  // Bulunamayan uniformlar da -1 ile önbelleğe alınır; tekrar sorgu yapılmaz,
  // uyarı yalnızca bir kez loglanır (hot-path log'unu önler).
  mUniformCache.emplace(name, kLoc);
  if (kLoc == -1) {  // NOLINT(readability-magic-numbers) — GL sentinel value
    spdlog::warn("[ShaderProgram] Uniform '{}' bulunamadı (warn-once).", name);
  }
  return kLoc;
}

void ShaderProgram::SetUniformMat3(const std::string& name,
                                   const float* mat3) const {
  const int32_t kLoc = GetUniformLocation(name);
  if (kLoc == -1) {
    return;
  }
  // Transform sınıfı satır-büyük (row-major) düzende veri tutar; OpenGL
  // varsayılan olarak sütun-büyük bekler, bu nedenle transpose=GL_TRUE.
  glUniformMatrix3fv(kLoc, 1, GL_TRUE, mat3);
}

void ShaderProgram::SetUniformMat4(const std::string& name,
                                   const float* mat4) const {
  const int32_t kLoc = GetUniformLocation(name);
  if (kLoc == -1) {
    return;
  }
  glUniformMatrix4fv(kLoc, 1, GL_FALSE, mat4);
}

void ShaderProgram::SetUniformVec4(const std::string& name, float r, float g,
                                   float b, float a) const {
  const int32_t kLoc = GetUniformLocation(name);
  if (kLoc == -1) {
    return;
  }
  glUniform4f(kLoc, r, g, b, a);
}

void ShaderProgram::SetUniformFloat(const std::string& name,
                                    float value) const {
  const int32_t kLoc = GetUniformLocation(name);
  if (kLoc == -1) {
    return;
  }
  glUniform1f(kLoc, value);
}

void ShaderProgram::SetUniformInt(const std::string& name,
                                  int32_t value) const {
  const int32_t kLoc = GetUniformLocation(name);
  if (kLoc == -1) {
    return;
  }
  glUniform1i(kLoc, value);
}

}  // namespace sdl_painter
