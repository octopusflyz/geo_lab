#include "tinyexr.h"
#include "texture_image.h"

// ===== 静态成员初始化 =====
TextureImage::Texture::Name2Texture TextureImage::Texture::allTexture;  // 全局纹理映射初始化。
TextureImage::Texture TextureImage::Texture::error;  // 错误纹理对象初始化。
