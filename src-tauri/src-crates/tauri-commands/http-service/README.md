# HTTP Service Tauri Commands

这个 crate 提供了 Tauri 命令，用于在前端调用 S3 上传功能。

## 功能

- ✅ 上传字节数据到 S3（支持完整配置）
- ✅ 简化版字节上传（基本参数）
- ✅ 从文件路径上传

## 安装

这个 crate 已经包含在 workspace 中，在 `main.rs` 中导入即可。

## 使用方法

### 1. 在 Rust 中注册命令

在 `src/main.rs` 或其他模块中：

```rust
use snow_shot_tauri_commands_http_service::{
    upload_to_s3,
    upload_bytes_to_s3_simple,
    upload_file_to_s3,
};

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            // ... 其他命令
            upload_to_s3,
            upload_bytes_to_s3_simple,
            upload_file_to_s3,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

### 2. 在前端调用

#### 方式一：使用完整配置（推荐）

```typescript
import { invoke } from '@tauri-apps/api/tauri';

interface UploadToS3Request {
  endpoint: string;
  region: string;
  access_key_id: string;
  secret_access_key: string;
  bucket: string;
  path_prefix?: string;
  force_path_style: boolean;
  data: number[];  // 字节数组
  filename: string;
  content_type?: string;
}

async function uploadScreenshotToS3(imageData: Uint8Array, filename: string) {
  try {
    const url = await invoke<string>('upload_to_s3', {
      request: {
        endpoint: 'https://s3.amazonaws.com',
        region: 'us-east-1',
        access_key_id: 'YOUR_ACCESS_KEY_ID',
        secret_access_key: 'YOUR_SECRET_ACCESS_KEY',
        bucket: 'my-screenshot-bucket',
        path_prefix: 'screenshots/',
        force_path_style: false,
        data: Array.from(imageData),  // Uint8Array 转换为普通数组
        filename: filename,
        content_type: 'image/png'
      }
    });

    console.log('上传成功！URL:', url);
    return url;
  } catch (error) {
    console.error('上传失败:', error);
    throw error;
  }
}

// 使用示例
const canvas = document.getElementById('canvas') as HTMLCanvasElement;
canvas.toBlob(async (blob) => {
  if (blob) {
    const arrayBuffer = await blob.arrayBuffer();
    const uint8Array = new Uint8Array(arrayBuffer);
    const timestamp = Date.now();
    const url = await uploadScreenshotToS3(
      uint8Array, 
      `screenshot_${timestamp}.png`
    );
    console.log('图片已上传:', url);
  }
});
```

#### 方式二：使用简化版命令

```typescript
async function uploadBytesSimple(
  imageData: Uint8Array, 
  filename: string
) {
  const url = await invoke<string>('upload_bytes_to_s3_simple', {
    endpoint: 'https://s3.amazonaws.com',
    region: 'us-east-1',
    accessKeyId: 'YOUR_KEY',
    secretAccessKey: 'YOUR_SECRET',
    bucket: 'my-bucket',
    data: Array.from(imageData),
    filename: filename,
    contentType: 'image/png'
  });
  
  return url;
}
```

#### 方式三：从文件路径上传

```typescript
async function uploadFile(filePath: string) {
  const url = await invoke<string>('upload_file_to_s3', {
    endpoint: 'https://s3.amazonaws.com',
    region: 'us-east-1',
    accessKeyId: 'YOUR_KEY',
    secretAccessKey: 'YOUR_SECRET',
    bucket: 'my-bucket',
    pathPrefix: 'screenshots/',
    forcePathStyle: false,
    filePath: filePath,
    objectKey: null  // 使用文件名作为对象键
  });
  
  return url;
}
```

### 3. React 组件示例

```tsx
import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/tauri';

interface S3Config {
  endpoint: string;
  region: string;
  accessKeyId: string;
  secretAccessKey: string;
  bucket: string;
}

function UploadButton() {
  const [uploading, setUploading] = useState(false);
  const [uploadedUrl, setUploadedUrl] = useState<string | null>(null);

  // 从配置或环境变量读取 S3 配置
  const s3Config: S3Config = {
    endpoint: process.env.NEXT_PUBLIC_S3_ENDPOINT || 'https://s3.amazonaws.com',
    region: process.env.NEXT_PUBLIC_S3_REGION || 'us-east-1',
    accessKeyId: process.env.NEXT_PUBLIC_S3_ACCESS_KEY_ID || '',
    secretAccessKey: process.env.NEXT_PUBLIC_S3_SECRET_ACCESS_KEY || '',
    bucket: process.env.NEXT_PUBLIC_S3_BUCKET || 'my-bucket',
  };

  const handleUpload = async (file: File) => {
    setUploading(true);
    
    try {
      // 读取文件为字节数组
      const arrayBuffer = await file.arrayBuffer();
      const uint8Array = new Uint8Array(arrayBuffer);
      
      // 生成唯一文件名
      const timestamp = Date.now();
      const filename = `screenshot_${timestamp}_${file.name}`;
      
      // 上传到 S3
      const url = await invoke<string>('upload_to_s3', {
        request: {
          ...s3Config,
          path_prefix: 'screenshots/',
          force_path_style: false,
          data: Array.from(uint8Array),
          filename: filename,
          content_type: file.type || 'application/octet-stream'
        }
      });
      
      setUploadedUrl(url);
      console.log('上传成功:', url);
      
    } catch (error) {
      console.error('上传失败:', error);
      alert(`上传失败: ${error}`);
    } finally {
      setUploading(false);
    }
  };

  return (
    <div>
      <input
        type="file"
        accept="image/*"
        onChange={(e) => {
          const file = e.target.files?.[0];
          if (file) handleUpload(file);
        }}
        disabled={uploading}
      />
      
      {uploading && <p>上传中...</p>}
      
      {uploadedUrl && (
        <div>
          <p>上传成功！</p>
          <a href={uploadedUrl} target="_blank" rel="noopener noreferrer">
            查看图片
          </a>
        </div>
      )}
    </div>
  );
}

export default UploadButton;
```

## 配置不同的云服务

### AWS S3

```typescript
{
  endpoint: 'https://s3.amazonaws.com',
  region: 'us-east-1',
  access_key_id: 'YOUR_KEY',
  secret_access_key: 'YOUR_SECRET',
  bucket: 'my-bucket',
  force_path_style: false
}
```

### 阿里云 OSS

```typescript
{
  endpoint: 'https://oss-cn-hangzhou.aliyuncs.com',
  region: 'cn-hangzhou',
  access_key_id: 'YOUR_KEY',
  secret_access_key: 'YOUR_SECRET',
  bucket: 'my-bucket',
  force_path_style: false
}
```

### 腾讯云 COS

```typescript
{
  endpoint: 'https://cos.ap-guangzhou.myqcloud.com',
  region: 'ap-guangzhou',
  access_key_id: 'YOUR_KEY',
  secret_access_key: 'YOUR_SECRET',
  bucket: 'my-bucket',
  force_path_style: false
}
```

### MinIO

```typescript
{
  endpoint: 'http://localhost:9000',
  region: 'us-east-1',
  access_key_id: 'minioadmin',
  secret_access_key: 'minioadmin',
  bucket: 'my-bucket',
  force_path_style: true  // MinIO 需要路径风格
}
```

## 错误处理

```typescript
try {
  const url = await uploadScreenshotToS3(imageData, filename);
  console.log('成功:', url);
} catch (error) {
  // 错误信息已经是字符串格式
  if (error.includes('初始化 S3 服务失败')) {
    console.error('S3 配置错误，请检查密钥和端点');
  } else if (error.includes('上传文件失败')) {
    console.error('网络错误或权限不足');
  } else {
    console.error('未知错误:', error);
  }
}
```

## 安全建议

1. **不要在前端代码中硬编码密钥**
   - 使用环境变量
   - 或从后端 API 获取临时凭证

2. **使用临时凭证**
   - 考虑使用 STS（Security Token Service）
   - 限制凭证的有效期和权限

3. **服务端签名**
   - 敏感场景下，可以在服务端生成预签名 URL
   - 前端只需要将文件上传到预签名 URL

## 完整流程示例

```typescript
// 1. 捕获截图
async function captureAndUpload() {
  // 假设你有一个截图函数
  const imageData = await captureScreenshot();
  
  // 2. 生成文件名
  const timestamp = Date.now();
  const filename = `screenshot_${timestamp}.png`;
  
  // 3. 上传到 S3
  const url = await invoke<string>('upload_to_s3', {
    request: {
      endpoint: 'https://s3.amazonaws.com',
      region: 'us-east-1',
      access_key_id: process.env.S3_KEY!,
      secret_access_key: process.env.S3_SECRET!,
      bucket: 'screenshots',
      path_prefix: 'users/user123/',
      force_path_style: false,
      data: Array.from(imageData),
      filename: filename,
      content_type: 'image/png'
    }
  });
  
  // 4. 保存 URL 或显示给用户
  console.log('Screenshot URL:', url);
  
  // 5. 可选：复制到剪贴板
  await navigator.clipboard.writeText(url);
  
  return url;
}
```

## API 参考

### upload_to_s3

完整配置的上传命令。

**参数：**
- `request: UploadToS3Request` - 包含所有配置的请求对象

**返回：**
- `Promise<string>` - 上传后的 URL

### upload_bytes_to_s3_simple

简化版上传命令。

**参数：**
- `endpoint: string`
- `region: string`
- `access_key_id: string`
- `secret_access_key: string`
- `bucket: string`
- `data: number[]`
- `filename: string`
- `content_type?: string`

**返回：**
- `Promise<string>` - 上传后的 URL

### upload_file_to_s3

从文件路径上传。

**参数：**
- `endpoint: string`
- `region: string`
- `access_key_id: string`
- `secret_access_key: string`
- `bucket: string`
- `path_prefix?: string`
- `force_path_style: boolean`
- `file_path: string`
- `object_key?: string`

**返回：**
- `Promise<string>` - 上传后的 URL

