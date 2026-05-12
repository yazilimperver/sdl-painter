#pragma once

#include <cstdint>
#include <string>

namespace sdl_painter {

/// @brief OpenGL shader programı sarmalayıcı.
///
/// Vertex + fragment shader'ı derler, bağlar ve uniform yönetimini sağlar.
class ShaderProgram {
 public:
  ShaderProgram() = default;
  ~ShaderProgram();

  // Non-copyable, movable
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;
  ShaderProgram(ShaderProgram&&) noexcept;
  ShaderProgram& operator=(ShaderProgram&&) noexcept;

  /// @brief Shader kaynak kodlarından program oluştur.
  /// @return Derleme/bağlama başarılı mı?
  bool Build(const std::string& vert_src, const std::string& frag_src);

  /// @brief Shader dosyalarından program oluştur.
  bool BuildFromFile(const std::string& vert_path, const std::string& frag_path);

  /// @brief Programı aktif et.
  void Use() const;

  /// @brief Program geçerli mi?
  bool IsValid() const { return mProgramId != 0; }

  /// @brief Uniform'ları ayarla
  void SetUniformMat3(const std::string& name, const float* mat3) const;
  void SetUniformMat4(const std::string& name, const float* mat4) const;
  void SetUniformVec4(const std::string& name, float r, float g,
                      float b, float a) const;
  void SetUniformFloat(const std::string& name, float value) const;
  void SetUniformInt(const std::string& name, int32_t value) const;

 private:
  /// @brief Tek bir shader'ı derle.
  static uint32_t CompileShader(uint32_t type, const std::string& src);

  uint32_t mProgramId{0};
};

}  // namespace sdl_painter
