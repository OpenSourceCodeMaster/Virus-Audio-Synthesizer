# 🥋🔊 Virus Audio Synthesizer (Bytebeat Studio)

> *"I am the Open Source Master, single file .exes scare me—so you have to compile this noise machine yourself."*

Welcome to another 100% binary-free scroll from the **OpenSourceCodeMaster** dojo. 

Ever wondered how classic Windows GDI malware and retro glitch demos generate those terrifying, bit-crushed electronic screeches using pure math? This lightweight, native Win32 C++ desktop app lets you synthesize, tweak, preview, and export looping **bytebeat** audio in real time—without running a single sketchy `.exe` downloaded from the internet.

*(True to my sacred oath—and the tragic day I broke a `.exe` using only a computer mouse—there is **no pre-compiled executable** in this repository. You get the raw, transparent C++ source code to forge yourself.)*

---

### ⚠️ The Master's Two Golden Warnings
1. **Audit the Code First:** Whether my account is hacked or not, **always read the source code before compiling or running it.** You never truly know what is happening behind the screen—especially in a repo with "Virus" in the title.
2. **Protect Your Eardrums:** **LOWER YOUR VOLUME** before clicking **Preview Loop**. Bytebeat math produces harsh, raw 8-bit square/glitch waves at full amplitude. Your headphones and speakers have been warned.

---

### ✨ Features
* **3 Classic Bytebeat Algorithms:**
  1. *Classic GDI Bytebeat Screech*
  2. *Chaotic Siren / Glitch Alarm*
  3. *Heavy Bit-Crushed Distortion*
* **Real-Time Parameter Sliders:** Tweak **Bit Shift A** (`1–16`), **Bit Shift B** (`1–16`), **Bitwise Mask** (`1–255`), and **Sample Rate** (`4000 Hz – 22050 Hz`) on the fly.
* **Live Audio Loop Preview:** Instant playback powered by the native Windows Multimedia API (`winmm.lib`).
* **Seamless MP3 Export:** Renders and encodes a clean, looping 44.1 kHz (128 kbps) `.mp3` file using Windows Media Foundation (`mfplat` / `mfreadwrite`).
* **Zero External Dependencies:** Written in pure C++ and native Win32 API—no bloated frameworks required.

---

### 🛠️ How to Build & Run
Because single-file `.exe` releases terrify me, you will need to compile the C++ source code yourself on Windows using **Visual Studio** or the **MSVC Command Line**.

👉 Read **[`instructions.txt`](./instructions.txt)** for step-by-step compilation instructions.

*(Quick preview for Developer Command Prompt users:)*
```cmd
cl /EHsc /O2 /DUNICODE /D_UNICODE main.cpp user32.lib gdi32.lib
```

---

### 📜 License
Licensed under the **MIT License**. Provided *"AS IS"*—if you crank your headphones to 100% and hit **Preview Loop**, the Open Source Master takes zero responsibility for your startled neighbors.
