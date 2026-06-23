# OV5640 MIPI UVC 使用文档

## 概览

本分支面向 Titan Board Mini 的 OV5640 MIPI 摄像头接入与 USB UVC 输出。当前代码包含两个样例：

- `samples/ov5640_af_mipi_probe/`：OV5640 传感器探测样例，用于确认 XCLK、I2C/SCCB、设备就绪、格式能力和部分控制项。
- `samples/titan_uvc_webcam/`：UVC 摄像头样例，用于从 OV5640 经 RA8P1 MIPI CSI/VIN 路径采集图像，并通过 USB UVC 暴露给主机。

当前主路径固定为 `320x240`、`RGB565`。摄像头侧使用 3 个 SDRAM video buffer，UVC USB 侧使用 copied-payload `net_buf` 池，当前样例配置为 `CONFIG_USBD_VIDEO_NUM_BUFS=320`。样例通过构建期生成的 project-local Zephyr USB override 修复重复预览路径，不修改 `ZEPHYR_BASE` 上游源码树。

## 当前验证状态

2026-06-23 在 Titan Board Mini CM85 上完成 `samples/titan_uvc_webcam/` 的真实刷写、主机侧重复采集和 `ffplay` 预览 smoke test。验证范围只覆盖 `320x240`、`RGB565`、UVC bulk/streaming 路径。

| 项目 | 结果 |
|---|---|
| 构建产物 | `build-titan_uvc_webcam/zephyr/zephyr.hex` |
| 烧录 | J-Link，目标 `R7KA8P1KF_CPU0`，刷写后复位运行 |
| 主机枚举 | `2fe3:0012 NordicSemiconductor Titan Board Mini UVC` |
| V4L2 设备名 | `Titan Board Mini UVC` |
| 格式 | `RGBP` / `320x240` / `153600` bytes per frame |
| 重复采集 | 两次连续 30 帧采集均成功，每次 `4608000` bytes |
| PNG 抽帧 | FFmpeg `rgb565le` 单帧 PNG 成功，图像非恒定 |
| 预览 smoke test | `ffplay` 以 `rgb565le` 打开 Titan Board Mini UVC capture 节点并持续收到帧 |
| 预览后重开 | 退出 `ffplay` 后再次 30 帧采集成功 |
| 150 帧采集 | 成功输出 `23040000` bytes |

未验证：自动对焦、不同分辨率、长时间运行、断连重连、WebRTC、`guvcview`、系统相机应用、亮场画质和颜色准确性。部分 host 打开初期可见 V4L2/FFmpeg error/corrupted 标记，但后续帧会恢复为完整 `153600` bytes；这仍是产品化前需要收敛的启动瞬态。当前 USB VID 仍使用 Zephyr 测试默认值 `0x2fe3`，量产前必须替换为正式 VID/PID。

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
| 摄像头视频缓冲 | 3 个 SDRAM buffer，128 字节对齐 |
| UVC USB 缓冲 | copied-payload `net_buf` pool，`CONFIG_USBD_VIDEO_NUM_BUFS=320` |

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
  - 分配 SDRAM 视频缓冲并在 camera 和 UVC 之间转交 buffer。

- `src/drivers/video/renesas_ra_mipi/video_titan_ra_mipi_csi.c`
  - 本地 RA8P1 MIPI CSI/VIN video 驱动。
  - 固定输出 `320x240 RGB565`。
  - 初始化 OV5640 MIPI/VIN 相关寄存器。
  - 处理 VIN frame-complete 回调、cache invalidate/flush、buffer copy 和丢帧计数。


- `samples/titan_uvc_webcam/cmake/titan_zephyr_overrides.cmake`
  - 在构建期生成 project-local Zephyr USB override 源文件。
  - 从 Zephyr CMake target 中移除对应上游源文件，改用 build 目录中的生成文件。
  - 若上游源文件结构漂移导致替换失败，CMake 会直接报错，避免静默编译错误实现。
  - 上游输入路径：`subsys/usb/device_next/class/usbd_uvc.c` 和 `drivers/usb/udc/udc_renesas_ra.c`。
  - 生成输出路径：`<build-dir>/titan_zephyr_overrides/subsys/usb/device_next/class/usbd_uvc.c` 和 `<build-dir>/titan_zephyr_overrides/drivers/usb/udc/udc_renesas_ra.c`。
  - 生成规则依赖 override generator 和两个上游源文件；非 pristine 增量构建也会在这些输入改变后重新生成。

- `scripts/generate_zephyr_overrides.py`
  - 从当前 `ZEPHYR_BASE` 读取 UVC class 和 Renesas RA UDC 源文件。
  - 生成带有本项目重复预览修复的 build-local 源文件。
  - 不写入、不修改 Zephyr checkout。
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

烧录。若已激活包含 `west` 的 Zephyr Python 环境，使用 `west flash`；使用 CMake 生成的 build 目录时，也可以直接运行 `ninja` 的 flash 目标：

```sh
west flash -d <build-dir> --runner jlink
ninja -C <build-dir> flash
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

主机侧应看到一个 UVC 设备，产品名为 `Titan Board Mini UVC`。当前已验证 `v4l2-ctl` 采集和 `ffplay` 以 `rgb565le` 预览；系统相机、浏览器 WebRTC、`guvcview` 等其他应用兼容性仍未验证，只能作为额外探索。

UVC 样例运行顺序：

1. 初始化 camera video device。
2. 初始化 UVC device。
3. 等待主机选择格式。
4. 配置 OV5640/VIN 输出。
5. 分配 3 个 SDRAM video buffer。
6. 启动 video stream。
7. 在 camera 和 UVC 之间循环转交 buffer。

### 3. 主机侧验证命令

烧录 UVC 样例后，用下面的命令确认枚举、格式和实际帧数据。不要硬编码具体 `/dev/video*` 节点号：节点号会随主机枚举顺序变化。

```sh
lsusb
v4l2-ctl --list-devices
v4l2-ctl -d /dev/videoX --all
v4l2-ctl -d /dev/videoX --list-formats-ext
v4l2-ctl -d /dev/videoX \
  --set-fmt-video=width=320,height=240,pixelformat=RGBP \
  --stream-mmap=3 --stream-count=30 --stream-to=<capture.raw> --verbose
```

把 `/dev/videoX` 替换为 `Titan Board Mini UVC` 对应的 video capture 节点；不要选 metadata capture 节点。重复打开也应满足：

- `VIDIOC_STREAMON` 成功。
- `cap dqbuf` 连续返回，`seq` 递增。
- 稳态帧的 `bytesused` 为 `153600`。
- 30 帧输出文件大小为 `30 * 320 * 240 * 2 = 4608000` bytes；150 帧输出文件大小为 `23040000` bytes。
- 抽取单帧后不是全 `0x00` 或全 `0xff`，并能渲染为实际场景。

FFmpeg 单帧抽取命令：

```sh
ffmpeg -f v4l2 \
  -input_format rgb565le \
  -video_size 320x240 \
  -framerate 30 \
  -i /dev/videoX \
  -frames:v 1 -y <frame.png>
```

### 4. ffplay 预览命令

本机已验证 `ffplay` 可用的输入格式是 FFmpeg 像素格式名 `rgb565le`；V4L2 fourcc `RGBP` 会被 `ffplay` 拒绝。

```sh
ffplay -f v4l2 \
  -input_format rgb565le \
  -video_size 320x240 \
  -framerate 30 \
  -fflags nobuffer \
  -flags low_delay \
  -framedrop \
  /dev/videoX
```

把 `/dev/videoX` 替换为 `Titan Board Mini UVC` 对应的 capture 节点；实际节点会随主机枚举顺序变化。

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

- 当前 Runtime Verified 范围只覆盖 Titan Board Mini 上的 `320x240 RGB565` UVC bulk path。
- 当前本地 MIPI CSI/VIN 驱动只发布 `320x240 RGB565`。
- UVC 样例按最大帧大小 `320 * 240 * 2` 限制 buffer。
- `ffplay`/FFmpeg 必须使用 `-input_format rgb565le`；`RGBP` 是 V4L2 fourcc，不是 FFmpeg input format 名。
- 打开流的前几帧可能出现 V4L2/FFmpeg error/corrupted 标记；后续帧在本次验证中恢复为完整帧。产品化前需要继续收敛。
- 样例已移除未验证的视频编码器分支；当前只维护 camera 到 UVC 的直接路径。
- 系统相机、浏览器 WebRTC、`guvcview`、断连重连、长时间运行、自动对焦和亮场画质仍未验证。
