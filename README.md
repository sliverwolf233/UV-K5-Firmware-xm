# xm-k5-firmware · UV-K5 中文 HAM 定制固件

以 [LOSEHU](https://github.com/losehu/uv-k5-firmware-custom)（uv-k5-firmware-custom）为基座，为**已扩容 2Mib EEPROM** 的原版 Quansheng UV-K5/K6 打造的中文 HAM 手台固件：MDC 信令 + 三层应急警报 + HAM 合规 TX + Live-VFO 找频，砍掉一切不用的。

> 本固件不以 LOSEHU 名义分发。修改清单与许可说明见 [NOTICE](./NOTICE)。

## 功能一览

**自研核心 —— 三层应急警报**（菜单 SOS 项四档：关 / 仅本地 / 仅远程 / 本地+远程，默认本地+远程）

| 层 | 机制 | 受众 |
|---|---|---|
| L1 本地啸叫 | 500Hz 起扫频循环，至手动取消 | 自己 + 周围耳朵 |
| L2 MDC 紧急帧 | op=0x00 发射 + 对方弹窗显示联系人名 + op=0x20 自动回执 | 刷社区固件的 K5 |
| L3 ZVEI 五音 | 警报序列 21414 发射（移植自 Dondji/BD1AHN rega.c） | 所有人耳朵（含原厂机/扫频者） |

触发时序：**TX 频率白名单先行**（被拦截则 L1 照响、屏幕显示"禁止发射"、PA 门控强制关闭零射频泄漏）→ 1kHz×3 预备音 → 按档位执行。出厂默认**侧键1长按**触发（可在侧键菜单改绑）。

**移植功能**
- Live-VFO 找频（移植自 uvk5cec/KD8CEC）：频率步进实时收听 + 迷你频谱，菜单 SEEK 项三档（关/监听/监听+频谱）
- 全字库中文界面（CHINESE_FULL=4）+ MDC1200 信令/联系人/可编辑 ID + FM 收音 + 频谱仪 + 一键扫频 + S 表等基座功能

**菜单体验（M4）**
- 底行右侧**分区徽章** [1]~[6]（1 无线电 / 2 信道 / 3 信令 / 4 DTMF / 5 显示按键 / 6 系统）
- 列表态**两位数字直达**任意菜单项（2 秒超时取消；子菜单编辑态数字语义不变）
- 跨分区导航短 beep 提示
- 主屏**同频自动折叠**：A/B 两守候信道完全一致（频率+信道+调制）时合一显示大字频率 + A/B 角标；任一槽位改动下一帧自动恢复双行

**按键映射（出厂默认，均可在菜单改）**

| 键位 | 功能 |
|---|---|
| 侧键1短按 / 长按 | 监听 / **紧急警报** |
| 侧键2短按 / 长按 | 手电 / 宽窄带 |
| F+5 / F+↑ / F+EXIT | 频谱 / 按键音 / 屏幕翻转 |
| 数字 0-9 长按 | 复制信道、切 A/B、切模式、对频、扫列表、功率、VOX、倒频、即呼、收音 |
| 长按 F / 长按 M | 键盘锁 / 调制切换 |

## 编译与下载

推送到 GitHub 后 Actions 自动构建（archlinux + arm-none-eabi-gcc 13.2，LTO），产物在 artifact **xm-firmware**（`XMK.bin`）。CI 内置 **60KB Flash 硬门禁**，超线自动打印最大符号清单。

本地编译（任意平台，与官方发布同源环境）：

```bash
make build        # 或使用 compile-with-docker.sh / Dockerfile_cn
```

固件尺寸：实测非 LTO **66,024 字节**（基座同配置 66,344B，xm 全部功能净增 +2,184B；LTO 链接收紧后过 60KB 线）。

## EEPROM 新增使用（2Mib 私有区 0x38000 起）

| 地址 | 用途 |
|---|---|
| 0x38000 | 紧急警报档位（1 字节） |
| 0x38003 | Live-VFO 模式（1 字节） |
| 0x38001/0x38002 | 预留（预备音开关/分区记忆） |

其余布局与基座一致，详见 [项目总规格与开发指南.md](./项目总规格与开发指南.md) §八。

## 与上游的差异

**砍除**（规格§二定案，理由见规格文档）：多普勒追频、双语切换、瀑布图、语音、NOAA、AIRCOPY、开机密码、1750Hz、短信三件套、SI4732、RX/TX 计时、状态栏呼号、**T9 拼音输入法**（为 60KB 预算让路；中文菜单显示不受影响，走 EEPROM 全字库）。

**行为调整**：隐藏菜单区清零（频段解锁/参数复位/电池调压/电池大小为正常系统区菜单，RESET 有二次确认；PTT+侧键开机组合判断已删除）；区域白名单零改造（GB 档=中港 144-148+430-440）。

详细功能对照与验证记录：[验证清单.md](./验证清单.md)

## 致谢与上游

本固件站在以下项目之上，版权头均按原样保留（见 [NOTICE](./NOTICE)）：

- [losehu/uv-k5-firmware-custom](https://github.com/losehu/uv-k5-firmware-custom) — LOSEHU 基座
- [egzumer/uv-k5-firmware-custom](https://github.com/egzumer/uv-k5-firmware-custom) — 谱系基座
- [Dual Tachyon](https://github.com/DualTachyon) — 原厂逆向固件
- [OneOfEleven](https://github.com/OneOfEleven)、[joaquimorg](https://github.com/joaquimorg) — MDC1200、频谱等组件
- [KD8CEC](https://github.com/kd8cec) — Live-VFO 块（ceccommon.c）
- [markusb (BD1AHN 谱系)](https://github.com/markusb) — REGA ZVEI 五音（rega.c）

如果这个项目对您有帮助，欢迎打赏支持上游开发者：

这是：[打赏名单](https://losehu.github.io/payment-codes/#%E6%94%B6%E6%AC%BE%E7%A0%81) 非常感谢各位的支持！！！
