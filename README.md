Magic & Mayhem DirectSound Fix Patch
《魔法师传奇》DirectSound 修复补丁

A simple DirectSound wrapper to fix sound effect volume issues in *Magic & Mayhem*.

## The Problem | 问题描述

The game *Magic & Mayhem*, released in 1998, has an audio issue on modern operating systems due to its dated DirectSound implementation. When a player adjusts the in-game sound effect volume, the game modifies the master volume of the entire application, not just the individual sound effects. This can disrupt the audio experience by causing all sounds to be muted or behave unexpectedly.

1998年发售的游戏《魔法师传奇》(Magic & Mayhem) 因其较早的 DirectSound 技术实现，在现代操作系统上存在一个音频兼容性问题。当玩家在游戏内调节音效音量时，游戏会直接修改整个应用的“主音量”，而非单个音效的音量。这会导致所有声音被一同静音或音量异常，影响了正常的游戏体验。

## The Solution | 解决方案

This project solves the problem with a custom `dsound.dll` file that acts as a proxy between the game and the original system `dsound.dll`.

It intercepts the game's audio function calls, especially those for volume control. By managing the volume state internally, it ensures that the in-game slider behaves as expected without affecting the master application volume, thus achieving stable and independent control over sound effects.

本项目通过一个定制的 `dsound.dll` 文件来解决此问题。该文件会作为游戏和系统原始 `dsound.dll` 之间的“代理”(Proxy)。

它会拦截游戏发出的音频函数调用，特别是针对音量控制的函数。通过内部的音量状态管理，它能确保游戏内的音量滑块行为符合现代玩家的预期，不再错误地操作全局音量，从而实现稳定、独立的音效控制。

## Installation | 安装方法

1.  Download the `dsound.dll` file from the [Releases](https://github.com/MyuriRose/dswrapper-mm/releases) page.

2.  Copy the `dsound.dll` file into the root directory of your *Magic & Mayhem* game (the same folder that contains the game's `.exe` file).

3.  Start the game. The fix is applied automatically.

4.  从本项目的 [Releases](https://github.com/MyuriRose/dswrapper-mm/releases) 页面下载 `dsound.dll` 文件。

5.  将下载好的 `dsound.dll` 文件复制到《魔法师传奇》的游戏根目录下（即游戏主程序 `.exe` 所在的那个文件夹）。

6.  重新启动游戏即可，本补丁会自动生效。
