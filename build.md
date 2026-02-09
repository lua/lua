# 🛠️ دليل بناء لغة ياقوت الشامل
## Build Guide for Yaqout Programming Language

---

## 📋 جدول المحتويات
- [المقدمة](#-المقدمة)
- [المتطلبات الأساسية](#-المتطلبات-الأساسية)
- [البناء على Windows](#-البناء-على-windows)
- [البناء على Linux](#-البناء-على-linux)
- [البناء على macOS](#-البناء-على-macos)
- [خيارات البناء المتقدمة](#-خيارات-البناء-المتقدمة)
- [بنية الملفات المصدرية](#-بنية-الملفات-المصدرية)
- [اختبار البناء](#-اختبار-البناء)
- [استكشاف الأخطاء](#-استكشاف-الأخطاء)
- [بناء المكتبة الثابتة](#-بناء-المكتبة-الثابتة)
- [توزيع الإصدار](#-توزيع-الإصدار)

---

## 📖 المقدمة

لغة ياقوت مبنية على محرك Lua 5.5 المكتوب بلغة C. عملية البناء بسيطة ولا تتطلب أي مكتبات خارجية معقدة. يمكنك بناء المشروع بأمر واحد فقط!

### لماذا يُعتبر البناء سهلاً؟
- ✅ لا توجد تبعيات خارجية (No External Dependencies)
- ✅ كود C99 قياسي ونظيف
- ✅ يعمل على أي مترجم C حديث
- ✅ حجم الكود المصدري صغير (~1MB)

---

## 📦 المتطلبات الأساسية

### المترجمات المدعومة

| المترجم | الإصدار الأدنى | ملاحظات |
|---------|---------------|---------|
| **GCC** | 4.8+ | الموصى به لـ Linux |
| **MinGW-w64** | 8.0+ | الموصى به لـ Windows |
| **Clang** | 3.5+ | بديل ممتاز |
| **MSVC** | 2015+ | Visual Studio |
| **TCC** | 0.9.27+ | للبناء السريع |

### أدوات إضافية (اختيارية)

| الأداة | الغرض |
|--------|-------|
| **Make** | أتمتة البناء |
| **Git** | إدارة الإصدارات |
| **CMake** | بناء متعدد المنصات |

---

## 🪟 البناء على Windows

### الطريقة 1: باستخدام MinGW-w64 (الموصى بها)

#### الخطوة 1: تثبيت MinGW-w64
```powershell
# طريقة 1: عبر winget
winget install -e --id mingw-w64.mingw-w64

# طريقة 2: عبر Chocolatey
choco install mingw

# طريقة 3: التحميل اليدوي
# https://www.mingw-w64.org/downloads/
# اختر: x86_64-posix-seh
```

#### الخطوة 2: إضافة MinGW للـ PATH
```powershell
# أضف هذا المسار للـ System PATH:
# C:\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64\bin

# للتحقق:
gcc --version
```

#### الخطوة 3: بناء ياقوت
```powershell
# انتقل لمجلد المشروع
cd C:\Users\GOGO\Desktop\whatsapp-bot\yarout

# أمر البناء الكامل (سطر واحد)
gcc -O2 -std=c99 -DLUA_USE_WINDOWS -o yaqout.exe lua.c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c loadlib.c linit.c -lm
```

#### الأمر المنسق (للقراءة السهلة):
```powershell
gcc -O2 -std=c99 -DLUA_USE_WINDOWS -o yaqout.exe ^
    lua.c ^
    lapi.c ^
    lcode.c ^
    lctype.c ^
    ldebug.c ^
    ldo.c ^
    ldump.c ^
    lfunc.c ^
    lgc.c ^
    llex.c ^
    lmem.c ^
    lobject.c ^
    lopcodes.c ^
    lparser.c ^
    lstate.c ^
    lstring.c ^
    ltable.c ^
    ltm.c ^
    lundump.c ^
    lvm.c ^
    lzio.c ^
    lauxlib.c ^
    lbaselib.c ^
    lcorolib.c ^
    ldblib.c ^
    liolib.c ^
    lmathlib.c ^
    loslib.c ^
    lstrlib.c ^
    ltablib.c ^
    lutf8lib.c ^
    loadlib.c ^
    linit.c ^
    -lm
```

### الطريقة 2: باستخدام Visual Studio (MSVC)

#### الخطوة 1: فتح Developer Command Prompt
```batch
# ابحث عن "Developer Command Prompt for VS"
# أو شغّل:
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
```

#### الخطوة 2: البناء
```batch
cd C:\Users\GOGO\Desktop\whatsapp-bot\yarout

cl /O2 /DLUA_USE_WINDOWS /Fe:yaqout.exe ^
    lua.c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c ^
    lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c ^
    lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c ^
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c ^
    liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c ^
    lutf8lib.c loadlib.c linit.c
```

### الطريقة 3: سكربت بناء تلقائي

أنشئ ملف `build.bat`:
```batch
@echo off
echo ====================================
echo    بناء لغة ياقوت - Yaqout Build
echo ====================================

:: التحقق من وجود gcc
where gcc >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [خطأ] GCC غير موجود! قم بتثبيت MinGW-w64
    pause
    exit /b 1
)

echo [1/3] تنظيف الملفات القديمة...
if exist yaqout.exe del yaqout.exe
if exist *.o del *.o

echo [2/3] تجميع الكود المصدري...
gcc -O2 -std=c99 -DLUA_USE_WINDOWS -o yaqout.exe ^
    lua.c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c ^
    lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c ^
    lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c ^
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c ^
    liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c ^
    lutf8lib.c loadlib.c linit.c -lm

if %ERRORLEVEL% neq 0 (
    echo [خطأ] فشل البناء!
    pause
    exit /b 1
)

echo [3/3] التحقق من الناتج...
if exist yaqout.exe (
    echo.
    echo ====================================
    echo    تم البناء بنجاح! ✓
    echo ====================================
    echo.
    yaqout.exe -v
) else (
    echo [خطأ] الملف التنفيذي غير موجود!
)

pause
```

---

## 🐧 البناء على Linux

### الطريقة 1: باستخدام GCC

#### تثبيت المتطلبات
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential

# Fedora
sudo dnf groupinstall "Development Tools"

# Arch Linux
sudo pacman -S base-devel

# openSUSE
sudo zypper install -t pattern devel_basis
```

#### البناء
```bash
cd ~/yaqout

# البناء البسيط
gcc -O2 -std=c99 -DLUA_USE_LINUX -o yaqout \
    lua.c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c \
    lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c \
    lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c \
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c \
    liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c \
    lutf8lib.c loadlib.c linit.c -lm -ldl

# أو باستخدام make
make
```

### الطريقة 2: باستخدام Make

الملف `makefile` موجود في المشروع:
```bash
# بناء كامل
make

# تنظيف
make clean

# بناء مع معلومات التنقيح
make MYCFLAGS="-g -DLUAI_ASSERT"
```

### الطريقة 3: سكربت بناء

أنشئ ملف `build.sh`:
```bash
#!/bin/bash

echo "===================================="
echo "   بناء لغة ياقوت - Yaqout Build"
echo "===================================="

# التحقق من وجود gcc
if ! command -v gcc &> /dev/null; then
    echo "[خطأ] GCC غير موجود!"
    echo "قم بتثبيته: sudo apt install build-essential"
    exit 1
fi

# تحديد النظام
UNAME=$(uname -s)
case "$UNAME" in
    Linux*)  PLATFORM="-DLUA_USE_LINUX"; LIBS="-lm -ldl";;
    Darwin*) PLATFORM="-DLUA_USE_MACOSX"; LIBS="-lm";;
    *)       PLATFORM=""; LIBS="-lm";;
esac

echo "[1/3] تنظيف الملفات القديمة..."
rm -f yaqout *.o

echo "[2/3] تجميع الكود المصدري..."
gcc -O2 -std=c99 $PLATFORM -o yaqout \
    lua.c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c \
    lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c \
    lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c \
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c \
    liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c \
    lutf8lib.c loadlib.c linit.c $LIBS

if [ $? -ne 0 ]; then
    echo "[خطأ] فشل البناء!"
    exit 1
fi

echo "[3/3] التحقق من الناتج..."
if [ -f yaqout ]; then
    echo ""
    echo "===================================="
    echo "   تم البناء بنجاح! ✓"
    echo "===================================="
    echo ""
    chmod +x yaqout
    ./yaqout -v
else
    echo "[خطأ] الملف التنفيذي غير موجود!"
    exit 1
fi
```

```bash
# تشغيل السكربت
chmod +x build.sh
./build.sh
```

---

## 🍎 البناء على macOS

### تثبيت المتطلبات
```bash
# تثبيت Xcode Command Line Tools
xcode-select --install

# أو تثبيت GCC عبر Homebrew
brew install gcc
```

### البناء
```bash
cd ~/yaqout

# باستخدام Clang (الافتراضي)
clang -O2 -std=c99 -DLUA_USE_MACOSX -o yaqout \
    lua.c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c \
    lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c \
    lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c \
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c \
    liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c \
    lutf8lib.c loadlib.c linit.c -lm

# أو باستخدام make
make macosx
```

---

## ⚙️ خيارات البناء المتقدمة

### أعلام التحسين (Optimization Flags)

| العلم | الوصف | الاستخدام |
|-------|-------|-----------|
| `-O0` | بدون تحسين | للتنقيح |
| `-O1` | تحسين خفيف | توازن |
| `-O2` | تحسين قياسي | **الموصى به** |
| `-O3` | تحسين أقصى | أداء عالي |
| `-Os` | تحسين للحجم | ملف صغير |

### أعلام التنقيح (Debug Flags)

```bash
# بناء للتنقيح
gcc -g -O0 -DLUAI_ASSERT -std=c99 -o yaqout_debug ...

# بناء مع معلومات التنقيح الكاملة
gcc -g3 -ggdb -O0 -DLUAI_ASSERT -DLUA_USE_APICHECK -std=c99 -o yaqout_debug ...
```

### تعريفات المنصة (Platform Defines)

| التعريف | المنصة |
|---------|--------|
| `-DLUA_USE_WINDOWS` | Windows |
| `-DLUA_USE_LINUX` | Linux |
| `-DLUA_USE_MACOSX` | macOS |
| `-DLUA_USE_POSIX` | POSIX عام |
| `-DLUA_USE_C89` | توافق C89 |

### تعريفات خاصة

```bash
# تفعيل جميع الفحوصات الداخلية
-DLUAI_ASSERT

# فحص API
-DLUA_USE_APICHECK

# اختبارات الذاكرة الصعبة
-DHARDMEMTESTS

# اختبارات المكدس الصعبة
-DHARDSTACKTESTS

# التوافق مع Lua 5.3
-DLUA_COMPAT_5_3
```

### بناء 32-bit vs 64-bit

```bash
# بناء 32-bit
gcc -m32 -O2 -std=c99 -o yaqout32 ...

# بناء 64-bit (الافتراضي على أنظمة 64-bit)
gcc -m64 -O2 -std=c99 -o yaqout64 ...
```

---

## 📁 بنية الملفات المصدرية

### الملفات الأساسية (Core Files)

```
📂 Core (النواة)
├── lua.c          # نقطة الدخول الرئيسية (main)
├── lua.h          # الـ Header العام
├── luaconf.h      # إعدادات التهيئة
├── lualib.h       # تعريفات المكتبات
├── lauxlib.c      # مكتبة مساعدة
└── lauxlib.h
```

### الملفات اللغوية (Language Files)

```
📂 Lexer & Parser (المحلل اللغوي والنحوي)
├── llex.c         # ⭐ المحلل اللغوي (الكلمات المفتاحية العربية)
├── llex.h
├── lctype.c       # ⭐ أنواع الحروف (دعم العربية)
├── lctype.h
├── lparser.c      # المحلل النحوي
├── lparser.h
├── lcode.c        # توليد الـ Bytecode
└── lcode.h
```

### الآلة الافتراضية (Virtual Machine)

```
📂 VM (الآلة الافتراضية)
├── lvm.c          # تنفيذ التعليمات
├── lvm.h
├── lopcodes.c     # تعريفات الأوامر
├── lopcodes.h
├── lopnames.h     # أسماء الأوامر
└── ljumptab.h     # جدول القفز
```

### إدارة الذاكرة (Memory Management)

```
📂 Memory (الذاكرة)
├── lmem.c         # إدارة الذاكرة
├── lmem.h
├── lgc.c          # جامع المخلفات
├── lgc.h
├── lstate.c       # حالة المترجم
└── lstate.h
```

### المكتبات القياسية (Standard Libraries)

```
📂 Libraries (المكتبات) ⭐ معربة
├── lbaselib.c     # الدوال الأساسية (اطبع، نوع...)
├── lmathlib.c     # مكتبة الرياضيات
├── lstrlib.c      # مكتبة النصوص
├── ltablib.c      # مكتبة الجداول
├── liolib.c       # الإدخال/الإخراج
├── loslib.c       # مكتبة النظام
├── lcorolib.c     # الروتينات المساعدة
├── ldblib.c       # مكتبة التنقيح
├── lutf8lib.c     # مكتبة UTF-8
├── loadlib.c      # تحميل المكتبات
└── linit.c        # تهيئة المكتبات
```

### ملفات أخرى

```
📂 Other (أخرى)
├── lobject.c      # الكائنات الداخلية
├── lobject.h
├── lstring.c      # إدارة النصوص
├── lstring.h
├── ltable.c       # إدارة الجداول
├── ltable.h
├── lfunc.c        # إدارة الدوال
├── lfunc.h
├── ltm.c          # الـ Tag Methods
├── ltm.h
├── ldebug.c       # معلومات التنقيح
├── ldebug.h
├── ldo.c          # الاستدعاءات
├── ldo.h
├── ldump.c        # تصدير الـ Bytecode
├── lundump.c      # استيراد الـ Bytecode
├── lundump.h
├── lzio.c         # واجهة الإدخال
├── lzio.h
├── llimits.h      # الحدود والثوابت
├── lprefix.h      # بادئات النظام
└── ltests.c       # اختبارات داخلية
```

---

## 🧪 اختبار البناء

### اختبار سريع

```bash
# تحقق من الإصدار
./yaqout -v
# أو على Windows
yaqout.exe -v

# الناتج المتوقع:
# Yaqout 5.5 (based on Lua 5.5)
```

### اختبار تفاعلي

```bash
./yaqout
# ثم اكتب:
اطبع("مرحباً بالعالم!")
# Ctrl+D للخروج (Linux/Mac) أو Ctrl+Z (Windows)
```

### اختبار ملف

```bash
# أنشئ ملف اختبار
echo 'اطبع("البناء ناجح!")' > test.yq

# شغّل الملف
./yaqout test.yq
```

### اختبارات شاملة

```bash
# تشغيل جميع الاختبارات
./yaqout testes/all.lua

# اختبار محدد
./yaqout testes/strings.lua
./yaqout testes/math.lua
./yaqout testes/api.lua
```

### اختبار الكلمات العربية

أنشئ ملف `test_arabic.yq`:
```lua
-- اختبار الكلمات المفتاحية العربية
محلي س = 10
محلي ص = 20

دالة جمع(أ, ب)
    ارجع أ + ب
نهاية

إذا س < ص إذن
    اطبع("س أصغر من ص")
نهاية

لكل ي = 1, 5 افعل
    اطبع("العدد: " .. ي)
نهاية

اطبع("نتيجة الجمع: " .. جمع(س, ص))
اطبع("اختبار ناجح! ✓")
```

```bash
./yaqout test_arabic.yq
```

---

## 🔧 استكشاف الأخطاء

### خطأ: `gcc: command not found`

```bash
# Windows
# تأكد من إضافة MinGW\bin للـ PATH

# Linux
sudo apt install build-essential

# macOS
xcode-select --install
```

### خطأ: `undefined reference to 'dlopen'`

```bash
# أضف -ldl للأمر
gcc ... -lm -ldl
```

### خطأ: `cannot find -lm`

```bash
# Linux
sudo apt install libc6-dev

# تأكد أن المكتبة موجودة
ls /usr/lib/x86_64-linux-gnu/libm.*
```

### خطأ: `llex.c: invalid multibyte character`

```bash
# تأكد من ترميز الملفات UTF-8
file llex.c
# يجب أن يظهر: UTF-8 Unicode text

# إذا كان الترميز خاطئ:
iconv -f ISO-8859-1 -t UTF-8 llex.c > llex_utf8.c
mv llex_utf8.c llex.c
```

### خطأ: `Windows.h not found` (MSVC)

```batch
:: استخدم Developer Command Prompt
:: وليس PowerShell أو CMD العادي
```

### تحذير: `-Wconversion`

```bash
# هذه تحذيرات عادية في كود Lua
# يمكن تجاهلها أو إضافة:
gcc -Wno-conversion ...
```

### الحروف العربية لا تظهر

```bash
# Windows CMD
chcp 65001

# Windows PowerShell
[Console]::OutputEncoding = [Text.UTF8Encoding]::UTF8

# Linux/Mac
export LANG=en_US.UTF-8
```

---

## 📚 بناء المكتبة الثابتة

### بناء liblua.a (Static Library)

```bash
# الخطوة 1: تجميع ملفات الكائنات
gcc -c -O2 -std=c99 -DLUA_USE_LINUX \
    lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c \
    lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c \
    lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c \
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c \
    liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c \
    lutf8lib.c loadlib.c linit.c

# الخطوة 2: إنشاء المكتبة الثابتة
ar rcs libyaqout.a *.o

# الخطوة 3: اختبار الربط
gcc -o yaqout lua.c -L. -lyaqout -lm -ldl
```

### بناء المكتبة المشتركة (Shared Library)

```bash
# Linux
gcc -shared -fPIC -O2 -std=c99 -DLUA_USE_LINUX \
    -o libyaqout.so \
    lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c \
    lfunc.c lgc.c llex.c lmem.c lobject.c lopcodes.c \
    lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c \
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c \
    liolib.c lmathlib.c loslib.c lstrlib.c ltablib.c \
    lutf8lib.c loadlib.c linit.c -lm -ldl

# Windows (DLL)
gcc -shared -O2 -std=c99 -DLUA_USE_WINDOWS -DLUA_BUILD_AS_DLL \
    -o yaqout.dll \
    lapi.c lcode.c ... -lm
```

---

## 📦 توزيع الإصدار

### هيكل حزمة التوزيع

```
📦 yaqout-1.0-win64/
├── yaqout.exe           # الملف التنفيذي
├── library.yq           # المكتبات الإضافية
├── README.md            # التوثيق
├── LICENSE              # الترخيص
├── examples/            # أمثلة
│   ├── hello.yq
│   ├── calculator.yq
│   └── game.yq
└── vscode-yaqout/       # إضافة VS Code
```

### سكربت إنشاء الحزمة

```bash
#!/bin/bash
VERSION="1.0"
PLATFORM="linux64"

# إنشاء المجلد
mkdir -p "yaqout-${VERSION}-${PLATFORM}"
cd "yaqout-${VERSION}-${PLATFORM}"

# نسخ الملفات
cp ../yaqout .
cp ../library.yq .
cp ../README.md .
cp ../LICENSE .
cp -r ../examples .
cp -r ../vscode-yaqout .

# ضغط الحزمة
cd ..
tar -czvf "yaqout-${VERSION}-${PLATFORM}.tar.gz" "yaqout-${VERSION}-${PLATFORM}"

echo "تم إنشاء الحزمة: yaqout-${VERSION}-${PLATFORM}.tar.gz"
```

---

## 📊 جدول ملخص أوامر البناء

| النظام | الأمر |
|--------|-------|
| **Windows (MinGW)** | `gcc -O2 -std=c99 -DLUA_USE_WINDOWS -o yaqout.exe *.c -lm` |
| **Linux** | `gcc -O2 -std=c99 -DLUA_USE_LINUX -o yaqout *.c -lm -ldl` |
| **macOS** | `clang -O2 -std=c99 -DLUA_USE_MACOSX -o yaqout *.c -lm` |
| **Make** | `make` |

---

## 🎉 الخلاصة

بناء لغة ياقوت عملية بسيطة تتطلب:
1. ✅ مترجم C (GCC/Clang/MSVC)
2. ✅ أمر واحد للبناء
3. ✅ لا توجد تبعيات خارجية

**للمساعدة أو الإبلاغ عن مشاكل:**
- افتح Issue على GitHub
- راجع ملف `CONTRIBUTING.md`

---

**تم إعداد هذا الدليل بواسطة:**
**إسلام النشار - ستوديو النشار**
**الإصدار: 1.0 | فبراير 2026**
