<!--
MR başlığı [Conventional Commits](https://www.conventionalcommits.org/tr/v1.0.0/)
formatında olmalı:  feat(painter): add ...   fix(vulkan): handle ...
-->

## Özet

<!-- Bu MR ne yapıyor? 1-3 cümle. -->

## Motivasyon

<!-- Neden gerekli? Bağlı olduğu issue varsa referans et:  Closes #42 -->

## Değişiklik Türü

- [ ] `feat` — Yeni özellik
- [ ] `fix` — Hata düzeltme
- [ ] `refactor` — İç temizlik
- [ ] `docs` — Yalnızca doküman
- [ ] `test` — Test ekleme/düzenleme
- [ ] `chore` — Bakım (build, CI, vb.)
- [ ] `perf` — Performans
- [ ] `BREAKING CHANGE` — Public API'yi kıran değişiklik

## Test Planı

<!--
Bu değişikliği nasıl test ettin? Hangi backend'lerde (OpenGL/Vulkan),
hangi platformlarda (Linux/Windows), hangi build tipinde (Debug/Release/ASan)?
-->

- [ ] Yeni/güncellenmiş birim testleri eklendi
- [ ] `ctest --preset linux-debug` yerel olarak geçti
- [ ] ASan/UBSan ile temiz çıkış (uygun ise)
- [ ] Format kontrolü temiz (`./scripts/format-check.sh`)
- [ ] Manuel test / demo yapıldı (uygun ise)

## Ekran Görüntüsü / Demo

<!-- Görsel değişiklik içeriyorsa öncesi/sonrası ekle. -->

## Belgeler

- [ ] `CHANGELOG.md` `[Unreleased]` bölümüne girdi eklendi
- [ ] Doxygen yorumları güncel (public API için `/// @brief`)
- [ ] `doc/` veya `adr/` güncelleme gerekiyorsa yapıldı
- [ ] README güncellemesi (gerekiyorsa)

## Mimari / API Etkisi

<!-- Bu değişiklik ABI/API'yi kırıyor mu? Yeni bağımlılık ekledi mi? ADR gerekti mi? -->

- [ ] ABI kıran değişiklik yok
- [ ] Yeni bağımlılık eklenmedi (eklendiyse Conan'dan)
- [ ] Yeni bir ADR eklendi (mimari karar varsa)

## İlgili Issue

<!-- Closes #...  veya  See #... -->

/label ~mr
