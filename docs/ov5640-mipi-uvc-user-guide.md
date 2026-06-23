# OV5640 MIPI UVC 使用文档

## 概览

本分支面向 Titan Board Mini 的 OV5640 MIPI 摄像头接入与 USB UVC 输出。当前代码包含两个样例：

- `samples/ov5640_af_mipi_probe/`：OV5640 传感器探测样例，用于确认 XCLK、I2C/SCCB、设备就绪、格式能力和部分控制项。
- `samples/titan_uvc_webcam/`：UVC 摄像头样例，用于从 OV5640 经 RA8P1 MIPI CSI/VIN 路径采集图像，并通过 USB UVC 暴露给主机。

当前主路径固定为 `320x240`、`RGB565`、3 个视频缓冲区，缓冲区放在 `SDRAM` Zephyr memory region 中。

## 硬件连接和板级资源

关键板级资源：

| 功能 | 当前配置 |
|---|---|
| 传感器 | OV5640 |
| 摄像头控制总线 | `&iic0`，OV5640 地址 `0x3c` |
| XCLK | `pwm11` 通道 1，25 MHz |
| MIPI CSI 输入 | `mipi-csi-vin` 本地 video 设备 |
| CSI-2 数据线 | `data-lanes = <1 2>` |
| USB 输出 | Zephyr UVC device，产品名 `Titan Board Mini UVC` |
| 视频缓冲 | 3 个 SDRAM buffer，128 字节对齐 |

`src/boards/rt-thread/titan_board_mini/ov5640_camera_xclk.c` 提供板级 XCLK 支持，样例 overlay 负责打开 `pwm11`、`iic0`、OV5640 节点和 MIPI/VIN 节点。

## 软件结构

主要文件：

- `samples/ov5640_af_mipi_probe/src/main.c`
  - 检查 OV5640 设备是否 ready。
  - 读取 video capabilities 和当前格式。
  - 查询 test pattern、auto focus、absolute focus 控制项。
  - 通过 `ov5640_probe_result` 暴露探测状态，便于调试读取。

- `samples/titan_uvc_webcam/src/main.c`
  - 初始化 Zephyr UVC device。
  - 过滤并注册 UVC 支持格式。
  - 等待主机选择视频格式。
  - 配置摄像头/VIN 输出格式和帧间隔。
  - 分配 SDRAM 视频缓冲并在 camera、可选 encoder、UVC 之间转交 buffer。

- `src/drivers/video/renesas_ra_mipi/video_titan_ra_mipi_csi.c`
  - 本地 RA8P1 MIPI CSI/VIN video 驱动。
  - 固定输出 `320x240 RGB565`。
  - 初始化 OV5640 MIPI/VIN 相关寄存器。
  - 处理 VIN frame-complete 回调、cache invalidate/flush、buffer copy 和丢帧计数。

- `src/dts/bindings/video/titan,ra8p1-mipi-csi-vin.yaml`
  - 本地 MIPI CSI/VIN devicetree binding。

## 构建

探测样例：

```sh
cmake -S samples/ov5640_af_mipi_probe \
  -B <build-dir> -GNinja \
  -DBOARD=titan_board_mini/r7ka8p1kflcac/cm85 \
  -DZEPHYR_EXTRA_MODULES="$PWD" \
  -DPYTHON_EXECUTABLE=<venv>/bin/python \
  -DPython3_EXECUTABLE=<venv>/bin/python
ninja -C <build-dir>
```

UVC 样例：

```sh
cmake -S samples/titan_uvc_webcam \
  -B <build-dir> -GNinja \
  -DBOARD=titan_board_mini/r7ka8p1kflcac/cm85 \
  -DZEPHYR_EXTRA_MODULES="$PWD" \
  -DPYTHON_EXECUTABLE=<venv>/bin/python \
  -DPython3_EXECUTABLE=<venv>/bin/python
ninja -C <build-dir>
```

烧录：

```sh
west flash -d <build-dir> --runner jlink
```

## 使用流程

### 1. 先运行探测样例

先烧录 `samples/ov5640_af_mipi_probe/`。期望现象：

- 日志打印 `Titan Board Mini OV5640 MIPI probe`。
- OV5640 device ready。
- `video_get_caps()` 返回成功。
- 能打印至少一个格式能力。
- `video_get_format()` 返回当前格式。

如果探测失败，先检查：

1. OV5640 模组供电和排线方向。
2. `pwm11` 是否输出 25 MHz XCLK。
3. `&iic0` 是否启用，SCL/SDA 是否有上拉。
4. OV5640 地址是否为 `0x3c`。
5. reset/enable 类 GPIO 是否与实际模组一致。

### 2. 再运行 UVC 样例

烧录 `samples/titan_uvc_webcam/` 后，把 Titan Board Mini 的 USB device 口连接到主机。

主机侧应看到一个 UVC 设备，产品名为 `Titan Board Mini UVC`。打开系统相机、浏览器 WebRTC 测试页、`ffplay`、`guvcview` 或其他 UVC viewer，选择 `320x240` 格式预览。

UVC 样例运行顺序：

1. 初始化 camera video device。
2. 初始化 UVC device。
3. 等待主机选择格式。
4. 配置 OV5640/VIN 输出。
5. 分配 3 个 SDRAM video buffer。
6. 启动 video stream。
7. 在 camera 和 UVC 之间循环转交 buffer。

## 调试要点

### 主机看不到 UVC 设备

检查：

- `CONFIG_USB_DEVICE_STACK_NEXT=y`
- `CONFIG_USBD_VIDEO_CLASS=y`
- Overlay 中 `uvc` 节点存在且 compatible 为 `zephyr,uvc-device`。
- USB device 口和 Type-C mux 配置是否正确。
- 主机枚举日志是否出现 `Titan Board Mini UVC`。

### 能枚举但没有画面

检查：

- 主机是否选择 `320x240`。
- 日志是否打印 `The host selected format ...`。
- `video_set_compose_format()` 是否失败。
- `video_stream_start()` 是否失败。
- `fmt.size` 是否超过 `TITAN_UVC_FRAME_MAX_SIZE`。
- SDRAM buffer 是否分配成功。

### 画面黑屏、花屏或颜色异常

检查：

- OV5640 初始化寄存器是否已写入成功。
- MIPI CSI data lanes 是否与硬件连接一致。
- VIN 输出是否仍按 `RGB565` 解释。
- Cache invalidate/flush 是否覆盖 DMA/VIN 写入和 UVC 读取边界。
- 是否有 `dropped_no_app_buf`、`dropped_doneq_full`、`last_ret`、`last_fsp_err` 或 `last_sensor_ret` 异常。

### 帧率低或卡顿

当前实现是同步 buffer 转交流程，适合先证明链路正确。优化前先记录：

- 主机实际选择的帧间隔。
- `frames` 递增速度。
- `dropped_no_app_buf` 和 `dropped_doneq_full` 是否递增。
- SDRAM buffer 是否持续复用。
- 日志和调试读取是否影响采集循环。

## 已知限制

- 当前本地 MIPI CSI/VIN 驱动只发布 `320x240 RGB565`。
- UVC 样例按最大帧大小 `320 * 240 * 2` 限制 buffer。
- 默认没有启用视频编码器路径；有 encoder 时，样例中仍有硬编码 NV12 的 FIXME。
