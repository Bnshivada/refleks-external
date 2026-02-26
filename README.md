# 🚛 Refleks External v3.0
> ETS2 1.58 | C++ | ImGui | External  
> RPM Boost (0–10K bar) + No Damage + Hız Göstergesi

---

## 🖥️ Ekran Görüntüsü (konsept)

```
╔══════════════════════════════════════════╗
║  Refleks External v3.0 | ETS2 1.58      ║
║  [CONNECTED] eurotrucks2.exe            ║
║─────────────────────────────────────────║
║  Oyun Verileri                          ║
║  Hiz   : 87.3 km/h                     ║
║  RPM   : 1842                           ║
║  Yakit : 412.5 L                        ║
║  Hasar : 0%  ████████████ (yesil)       ║
║─────────────────────────────────────────║
║  RPM Boost    [x] Aktif                 ║
║  [████████████████░░░░] RPM: 7500       ║
║  ██████████████████░░░░ (bar)           ║
║  Hizli: [1K][3K][5K][7.5K][10K]        ║
║─────────────────────────────────────────║
║  No Damage    [x] Aktif                 ║
║  Hasar sifirlanıyor - Tır yıkılmaz!    ║
╚══════════════════════════════════════════╝
```

---

## ⚙️ Kurulum (Adım Adım)

### 1. Gerekli Araçlar
- [Visual Studio 2022](https://visualstudio.microsoft.com/) (C++ geliştirme araçları ile)
- [CMake 3.20+](https://cmake.org/download/)
- [Git](https://git-scm.com/)

### 2. Repoyu Klonla
```bash
git clone https://github.com/KULLANICI/refleks-external.git
cd refleks-external
```

### 3. ImGui İndir
```bash
# imgui klasörü oluştur
mkdir imgui

# imgui GitHub'dan kopyala:
# https://github.com/ocornut/imgui
# Şu dosyaları imgui/ klasörüne koy:
# imgui.h, imgui.cpp
# imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp
# imgui_impl_win32.h, imgui_impl_win32.cpp
# imgui_impl_dx11.h, imgui_impl_dx11.cpp
```

### 4. Derle
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 5. Çalıştır
```
1. ETS2'yi aç ve haritaya gir
2. RefleksExternal.exe'ye SAĞ TIK → Yönetici olarak çalıştır
3. INSERT → Menü aç/kapat
```

---

## ⌨️ Tuşlar

| Tuş | Fonksiyon |
|-----|-----------|
| INSERT | Menü aç/kapat |
| F5 | Max RPM sayacını sıfırla |

---

## 🗂️ Dosya Yapısı

```
refleks-external/
├── main.cpp          ← Ana kod + ImGui render
├── Memory.h          ← Memory okuma/yazma sınıfı
├── CMakeLists.txt    ← Derleme ayarları
├── imgui/            ← ImGui dosyaları (manuel ekle)
└── README.md
```

---

## ⚠️ Önemli Uyarılar

> **TruckersMP'de KESİNLİKLE KULLANMA → KESİN BAN**

- Sadece **Singleplayer** modunda kullan
- **Yönetici olarak** çalıştırılmazsa bağlanamaz
- ETS2 **açık ve haritada** olmalı
- Antivirüs uyarı verebilir → whitelist ekle (false positive)
- 1.58 dışı versiyonlarda pointer adresleri farklı olabilir

---

## 📺 Yapım Videosu

*[YouTube linki eklenecek]*

---

## 📋 Sürüm Geçmişi

| Versiyon | Özellik |
|----------|---------|
| v3.0 | C++ ImGui, RPM bar, No Damage, Hız göstergesi |
| v2.0 | Python pymem versiyonu |
| v1.0 | Python + SCS Telemetry versiyonu |
