# xm-k5-firmware 开发任务

## 角色
你是嵌入式固件工程师，为 Quansheng UV-K5（DP32G030，已扩容 2Mib EEPROM）开发基于 LOSEHU 的定制固件。

## 工作区（先读这些）
- 基座源码：D:/Codes/UV-K5-Firmware/uv-k5-firmware-custom/（只读参照，勿直接修改）
- 移植来源1：D:/Codes/UV-K5-Firmware/Dondji/App/app/rega.c 与 rega.h（ZVEI五音警报）
- 移植来源2：D:/Codes/UV-K5-Firmware/uvk5cec/ceccommon.c 与 ceccommon.h（Live-VFO迷你频谱）
- **权威规格：D:/Codes/UV-K5-Firmware/xm-k5-firmware/项目总规格与开发指南.md（必读，全部定案在此）**
- 历史详版（按需查）：D:/Codes/UV-K5-Firmware/我的固件功能配置单.md

## 开发目录
D:/Codes/UV-K5-Firmware/xm-k5-firmware/（固件代码放这里，从基座 fork 起步）

## 任务（严格按顺序，每步编译验证后再进下一步）

### M1 基线搭建
1. 将基座 uv-k5-firmware-custom 完整复制到 xm-k5-firmware/（保持目录结构）
2. 清理：删除 "h --force-with-lease origin main"、.idea/、payment/；font.c 原样不动；**GB2312 编码源文件绝不转码**
3. Makefile 按规格§二的固化块修改（全部用 = 强制赋值），PACKED_FILE_SUFFIX 改 XMxxx
4. Docker 编译（compile-with-docker.sh 或 Dockerfile_cn）成功出 bin
5. 报告实测固件 size（对照 60KB 红线）

### M2 移植（rega.c → Live-VFO）
6. 移植 rega.c/rega.h：改 include 路径（bsp/dp32g030/gpio.h 对齐基座），编译通过
7. 移植 ceccommon 的 Live-VFO：三个钩子点（主循环500ms片尾 / app/main.c 频率步进后 / 菜单加 MENU_LIVESEEK），注意 CommBuff 并发隐患

### M3 警报整合（核心自研）
8. SIDEFUNCTIONS[] 新增 ACTION_OPT_EMERGENCY，出厂默认绑侧键1长按
9. 菜单新增"紧急警报"四档（关/仅本地/仅远程/本地+远程），存 EEPROM 0x38000
10. 触发时序：TX_freq_check 先行（失败→L1照响+VFO_STATE_TX_DISABLE）→ 1kHz×3预备音 → 按档位发 L1/L2/L3
11. MDC op=0x00 紧急帧收发 + 联系人弹窗 + op=0x20 自动 ACK

### M4 体验
12. 菜单底行右侧分区徽章 [1无线电]（六区归属见规格§五）
13. 菜单列表态两位数字直达（=原版序号索引，2秒超时；子菜单编辑态不冲突）+ 跨分区 beep
14. 主屏同频自动折叠（VfosIdentical() 每帧判定，大字体用 EEPROM 0x02480 现成 gFontBigDigits）
15. 亚音合并：R_DCS/R_CTCS/T_DCS/T_CTCS → "数字亚音/模拟亚音"两项 + *键三态（收+发/仅收/仅发）+ 扫描回填
16. 隐藏区清零：频段解锁/参数复位(+gAskForConfirmation二次确认)/电池调压/电池大小转正常菜单；删 PTT+上侧键开机组合判断

## 纪律（每条都必须遵守）
- GB2312 文件不转码（中文字节=字库索引，转了全花）
- 新功能=新文件+少量钩子，不重排上游代码；新全局变量先 grep misc.h 查重
- 每完成一步 make 看 size（红线 60KB，预算见规格§一）
- 白名单拦截必须走 RADIO_SetVfoState(VFO_STATE_TX_DISABLE)
- 保留所有上游版权头；新文件加自己的 Apache-2.0 声明；维护 NOTICE
- 区域白名单零改造：GB 档=中港频段（144-148+430-440），仅可选改文案"GB HAM"→"中港HAM"
- 不做的：多普勒/双语/瀑布图/SI4732/语音/RX-TX计时/呼号显示（均已定案砍除，规格§二有理由）

## 完成标准
M1-M4 全部通过编译 + size 达标（≤60KB）+ 规格文档中全部功能可对照验证。从 M1 第 1 步开始。
