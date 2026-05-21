# Sunum: GCS ↔ Autopilot İletişim Sistemi

**Süre:** 5–10 dakika  
**Ne yapacağız:** Dosyaları göstereceğiz → sistemi kısaca anlatacağız → testleri çalıştırıp PASS sonuçlarını göstereceğiz.

---

## Sunum sırası (kısa)

1. Giriş — proje ne? (~1 dk)  
2. Dosyalar + önemli fonksiyonlar (~3 dk)  
3. Sistem şeması — GCS ve autopilot nasıl konuşur? (~1 dk)  
4. Canlı testler — terminalde 3–4 test (~3–4 dk)  
5. CI — GitHub’da otomatik test (~30 sn)  
6. Kapanış (~30 sn)

---

## Kısaltmalar — Sunumda hızlı bakış

Mentör veya dinleyici bir terim sorduğunda veya slaytta kısaltma geçtiğinde buradan tek cümle okuyabilirsin.

| Kısaltma | Açılımı | Tek cümle |
|----------|---------|-----------|
| **GCS** | Ground Control Station | Yer istasyonu — uçağı yerden izleyen ve komut veren taraf |
| **TCP** | Transmission Control Protocol | Bağlantılı, güvenilir kanal; komut ve nabız burada |
| **UDP** | User Datagram Protocol | Hızlı, bağlantısız kanal; sürekli telemetri burada |
| **ACL** | Access Control List | Kimin IP’si bağlanabilir — izin listesi |
| **ACK** | Acknowledgement | “Komutu aldım” onay cevabı |
| **HB** | Heartbeat | “Hâlâ bağlıyım” nabız sinyali |
| **CI** | Continuous Integration | Her kod gönderiminde otomatik derleme + test (GitHub Actions) |
| **CIDR** | — (ör. `192.168.1.0/24`) | Hangi IP aralığının izinli olduğu |
| **XOR checksum** | — | Paketin bozulup bozulmadığını kontrol eden özet bayt |
| **SAFE_MODE** | — | Bağlantı kopunca otopilotun geçtiği güvenli mod |
| **RTL** | Return to Launch | (Gerçek sistemlerde) eve dön modu — bizde SAFE_MODE benzeri rol |
| **Hz** | Hertz (saniyedeki tekrar) | 50 Hz = saniyede 50 telemetri paketi |
| **WSL** | Windows Subsystem for Linux | Windows’ta Ubuntu açıp projeyi Linux gibi çalıştırma |
| **PASS / FAIL** | — | Test geçti / geçmedi (CI log’undaki satırlar) |

**Sık geçen dosya / terimler (kısaltma değil ama sorulabilir):**

- **Thread** — Aynı program içinde paralel çalışan iş (biri dinler, biri gönderir).
- **Port** — Bilgisayardaki “kapı numarası” (5760 TCP, 5761 UDP, 5762 test).
- **Sunucu / istemci** — GCS dinler (sunucu), autopilot bağlanır (istemci).

---

## 1. Giriş — Mentöre ne söyleyeceğim?

Merhaba. Bu projede **yer istasyonu (GCS)** ile **otopilot**u iki ayrı program gibi çalıştırdık — tıpkı gerçekte iki farklı bilgisayar konuşması gibi.

GCS komutları alıyor ve cevap veriyor. Autopilot veri gönderiyor ve bağlantı koparsa güvenli moda geçiyor. Bunları **test scriptleri** ve **GitHub Actions** ile otomatik kontrol ediyoruz.

Şimdi dosyalara bakalım, sonra testleri canlı çalıştıralım.

---

## 2. Dosyalar ve önemli fonksiyonlar

[Repo’yu aç. Her dosyada aşağıdaki fonksiyon isimlerini IDE’de gösterebilirsin — mentöre “bu dosya şunları yapıyor” demek için yeterli.]

### Proje kökü

**`CMakeLists.txt`**  
*Ne işe yarar?* Projenin “derleme tarifi”dir. Hangi `.cpp` dosyalarının hangi programa gireceğini ve `gcs` / `autopilot` ikilisinin nasıl üretileceğini tanımlar. Sunumda açmak şart değil; “derlemeyi CMake yönetiyor” demen yeterli.

**`README.md`**  
*Ne işe yarar?* Projeyi ilk kez açan biri için rehber: nasıl derlenir, iki program nasıl başlatılır, testler nasıl koşulur. Mentör veya jüri repo’ya baktığında buradan devam eder.

**`.github/workflows/ci.yml`**  
*Ne işe yarar?* GitHub’a her push’ta otomatik test senaryosunu tanımlar. İnsan eli değmeden derleme ve dört testin koşmasını sağlar; sunumda Actions ekranının “neden yeşil” olduğunu buraya bağlayabilirsin.

---

### `gcs_main.cpp` — Yer istasyonu

*Bu dosya ne işe yarar?* GCS programının kalbidir; `main` burada. Ağ detayına girmez — `NetworkLayer` ve `GCSLogic`’i kullanır. Bağlantı kabul eder, komutlara cevap verir, telemetri toplar, nabız gönderir, testlerin sorduğu kontrol portunu açar ve IP listesine göre bağlantı reddeder. Yani “yerden uçak bilgisayarına ne sunuluyor?” sorusunun cevabı bu dosyada akar.

| Fonksiyon | Ne yapar? |
|-----------|-----------|
| `main` | Argümanları okur, thread’leri başlatır, Ctrl+C ile kapatır |
| `parse_args` | `--tcp-port`, `--udp-port`, `--subnet`, `--allow-localhost` vb. |
| `parse_cidr` | `192.168.1.0/24` gibi ağı IP + maskeye çevirir |
| `is_whitelisted` | Bağlanan IP izinli mi? (ACL) |
| `tcp_accept_loop` | Port 5760’ı dinler; her bağlantıda IP kontrolü, sonra `tcp_client_thread` |
| `tcp_client_thread` | Komut mu nabız mı ayırır; komuta ACK, nabıza (açıksa) cevap verir |
| `heartbeat_sender_loop` | Her 1 sn autopilota nabız gönderir (`STOP_HEARTBEAT` ile durur) |
| `udp_recv_loop` | UDP telemetri alır, `validate_telemetry` ile kontrol eder |
| `validate_telemetry` | XOR checksum doğru mu? |
| `stats_writer_loop` | Her 1 sn `/tmp/gcs_stats.txt` yazar |
| `control_port_loop` | Port 5762; test komutları: bozuk sayısı, nabız durdur/başlat |
| `handle_control_client` | `GET corrupted`, `STOP_HEARTBEAT`, `RESUME_HEARTBEAT` |

**Sunum cümlesi:** “GCS tarafında accept döngüsü, istemci işleyici, nabız gönderici ve UDP alıcı ayrı thread’lerde çalışıyor.”

---

### `autopilot_main.cpp` — Otopilot

*Bu dosya ne işe yarar?* Uçuş bilgisayarını simüle eden programın giriş noktasıdır. GCS’e TCP ile bağlanır; koparsa tekrar dener. Sürekli telemetri (50 Hz) ve nabız gönderir; ayrı bir gözetleyici thread bağlantı çok uzun kesilirse güvenli moda geçirir. Durumu dosyaya yazar ki testler `SAFE_MODE`’u okuyabilsin. GCS’ten gelen ACK’leri de burada loglanır.

| Fonksiyon | Ne yapar? |
|-----------|-----------|
| `main` | Thread’leri başlatır (TCP, nabız, UDP, watchdog) |
| `connect_with_retry` | GCS hazır değilse 500 ms arayla tekrar bağlanır |
| `tcp_recv_loop` | GCS’ten nabız ve ACK okur; kopunca yeniden bağlanır |
| `heartbeat_sender_loop` | Her 1 sn GCS’e nabız gönderir |
| `udp_telemetry_loop` | Her 20 ms sahte hız/irtifa telemetri gönderir (50 Hz) |
| `watchdog_loop` | 3 sn nabız yoksa `SAFE_MODE`; her 500 ms durum dosyası yazar |
| `write_state_file` | `/tmp/autopilot_state.txt` → `state=IDLE` / `SAFE_MODE` vb. |
| `make_heartbeat_packet` | Nabız paketini doldurur |

**Sunum cümlesi:** “Otopilot bir yandan sürekli telemetri yollar, bir yandan nabız bekler; 3 saniye gelmezse güvenli moda geçer.”

---

### `network_layer/` — Ağ katmanı

*Klasörün rolü:* Uçuş mantığından bağımsız “ham iletişim” katmanı. GCS ve autopilot aynı soket ve paket tanımlarını paylaşır; böylece bir tarafta protokol değişince diğer taraf da aynı header’ları kullanır.

#### `TcpSocket.h` / `TcpSocket.cpp` — Güvenilir bağlantı (TCP)

*Bu dosya ne işe yarar?* TCP’nin teknik işlerini tek yerde toplar: sunucu tarafında dinle ve kabul et, istemci tarafında bağlan, veriyi eksiksiz gönder/al, kapat. Komut, onay ve nabız paketleri bu sınıf üzerinden gider; üst katman paketin içeriğine bakmaz.

| Fonksiyon | Ne yapar? |
|-----------|-----------|
| `bind_and_listen` | Sunucu: portu aç, dinlemeye başla (GCS) |
| `accept_connection` | Gelen bağlantıyı kabul et, istemci IP’sini ver |
| `connect_to` | İstemci: GCS’e bağlan (autopilot) |
| `send_all` / `recv_all` | Veriyi parça parça değil, tam gönder/al |
| `close_fd` | Bağlantıyı kapat |

#### `UdpSocket.h` / `UdpSocket.cpp` — Hızlı paket (UDP)

*Bu dosya ne işe yarar?* Telemetri gibi sık ve küçük mesajlar için UDP soketini yönetir. GCS dinler ve alır; autopilot gönderir. Alım tarafında kısa bekleme vardır — program kapanırken thread’ler takılmasın diye.

| Fonksiyon | Ne yapar? |
|-----------|-----------|
| `bind_port` | UDP portunu dinle (GCS) |
| `send_to` | Belirli adrese paket gönder (autopilot) |
| `recv_from` | Paket al; kısa timeout ile thread düzgün kapansın |
| `ensure_open` | Göndermeden önce soketi aç (istemci tarafı) |

#### `NetworkLayer.h` / `NetworkLayer.cpp` — Paketler ve ortak kurallar

*Bu dosya ne işe yarar?* Tüm sistemin “dilini” tanımlar: dört paket tipinin byte düzeni burada. Ayrıca telemetri için XOR checksum ve ACL için “bu IP bu ağda mı?” kontrolü burada. TcpSocket ve UdpSocket’i bir arada sunan ince bir cephe de var; asıl iş yine alt sınıflarda.

| Öğe | Ne yapar? |
|-----|-----------|
| `FlightModeCommand` | Uçuş komutu paketi (sıra no + mod) |
| `CommandAck` | “Komutu aldım” cevabı |
| `TelemetryPacket` | Hız, irtifa + checksum |
| `HeartbeatPacket` | Nabız (zaman damgalı) |
| `compute_xor_checksum` | Telemetri bütünlük kontrolü |
| `is_ip_in_subnet` | IP, izin verilen ağda mı? (ACL’nin kalbi) |

**Sunum cümlesi:** “Ağ katmanı sadece kabloyu yönetiyor; paket şekilleri ve checksum burada tanımlı.”

---

### `flight_logic/` — Uçuş mantığı

*Klasörün rolü:* Ağdan gelen baytları “anlamlı uçuş bilgisine” çevirir. Soket açmaz; `gcs_main` ve `autopilot_main` bu modülü çağırır.

#### `FlightLogic.h` / `FlightLogic.cpp`

*Bu dosya ne işe yarar?* İki mantıksal bileşen içerir: otopilotun durum makinesi (normal / güvenli mod) ve GCS’in telemetri kaydı ile bozuk paket sayacı. Testlerin okuduğu istatistik dosyası da bu sayaç üzerinden dolar.

#### Sınıf `Autopilot` (otopilot tarafı)

| Fonksiyon / alan | Ne yapar? |
|------------------|-----------|
| `set_state` / `get_state` | IDLE, FLYING, SAFE_MODE geçişi (log yazar) |
| `last_heartbeat_ms` | Son nabız zamanı (watchdog bunu okur) |

#### Sınıf `GCSLogic` (yer istasyonu tarafı)

| Fonksiyon | Ne yapar? |
|-----------|-----------|
| `on_telemetry_received` | Paket geçerliyse kaydet; değilse sayacı artır |
| `get_corrupted_count` | Kaç bozuk paket geldi (test bunu sorar) |
| `write_stats_to_file` | `corrupted_packet_count=N` dosyaya yazar |

**Sunum cümlesi:** “FlightLogic, ham paketin anlamını yorumluyor: durum makinesi ve bozuk veri sayacı burada.”

---

### `platform_posix.h`

*Bu dosya ne işe yarar?* Linux ağ başlıklarını ve küçük yardımcıları tek yerden include eder. Nabız paketindeki uzun zaman alanını ağ byte sırasına çevirir; log satırları için okunabilir saat üretir. Ana mantık değil ama tüm modüllerin ortak “altyapı” dosyasıdır.

| Fonksiyon | Ne yapar? |
|-----------|-----------|
| `host_to_be64` | 64 bit zamanı ağ formatına çevirir (nabız paketi) |
| `platform_localtime` | Log için okunabilir tarih/saat |

---

### `tests/` — Test scriptleri

*Klasörün rolü:* C++ programları çalışırken dışarıdan “müşteri gibi” davranır; PASS/FAIL ile ödev kriterlerini otomatik doğrular. Sunumda bu dosyaları terminalden çalıştırırsın.

**`test_tcp_commands.py`**  
*Ne işe yarar?* GCS’e 100 uçuş komutu gönderir; her birine doğru onay (ACK) gelip gelmediğini sayar. Komut kanalının güvenilir olduğunu kanıtlar.

**`test_udp_checksum.py`**  
*Ne işe yarar?* UDP ile geçerli ve bilerek bozuk telemetri yollar; GCS’in bozukları sayıp saymadığını kontrol portundan sorar. Veri bütünlüğü (checksum) testidir.

**`test_heartbeat.py`**  
*Ne işe yarar?* GCS’e “nabız göndermeyi durdur” der; otopilotun yaklaşık 3 saniye içinde güvenli moda yazıp yazmadığını dosyadan okur. Hata toleransı testidir.

**`test_acl.py`**  
*Ne işe yarar?* GCS kısıtlı ağ ile çalışırken localhost’tan bağlanmayı dener; bağlantının hemen kapanmasını bekler. IP güvenliği testidir.

**`test_all.py`**  
*Ne işe yarar?* Yukarıdaki scriptleri tek tek subprocess ile koşturur; özet PASS/FAIL tablosu basar. ACL için GCS restart’ı kendi yapmaz — CI veya sen ayrı adımda yaparsın.

| Dosya | Önemli fonksiyon / akış |
|-------|-------------------------|
| `test_tcp_commands.py` | `send_command`, `recv_ack` |
| `test_udp_checksum.py` | `make_telemetry` (checksum üretir) |
| `test_heartbeat.py` | `STOP_HEARTBEAT` → `SAFE_MODE` süresi |
| `test_acl.py` | Bağlan → süre ölç → 50 ms altı mı? |

---

### `build/` (derlemeden sonra)

*Ne işe yarar?* `cmake --build` sonrası oluşan klasör; kaynak kod değil, **çalıştırılabilir programlar** burada.

**`build/gcs`** — `gcs_main.cpp`’nin derlenmiş hali; terminalde `./build/gcs` ile başlatırsın.

**`build/autopilot`** — `autopilot_main.cpp`’nin derlenmiş hali; `./build/autopilot` ile GCS’e bağlanır.

[İpucu: Sunumda bir dosyayı açıp tek bir fonksiyon göstermek yeterli — örneğin `is_whitelisted` veya `watchdog_loop`.]

---

## 3. Sistem nasıl kuruldu?

[Şemayı göster.]

```
   GCS (yer istasyonu)              Autopilot
   ─────────────────                ─────────
   TCP 5760  ◄── komut, nabız ──►   GCS'e bağlanır
   UDP 5761  ◄── telemetri ─────    sürekli veri gönderir
   TCP 5762  ◄── test komutları
```

**Basitçe:**

- **GCS** dinler, **autopilot** bağlanır.
- Önemli mesajlar **TCP** ile (kaybolmasın diye).
- Sürekli uçuş verisi **UDP** ile (hızlı olsun diye).
- Bağlantı kopunca autopilot yaklaşık **3 saniye** sonra **SAFE_MODE** (güvenli mod) yazar.

Kod iki parçaya ayrıldı: biri “ağ”, biri “mantık”. Böylece okunması ve test edilmesi kolay.

---

## 4. Canlı demo — Önce programları çalıştır

[Linux veya WSL. İki terminal.]

Derleme (bir kez):

```bash
cmake -B build && cmake --build build
```

**Terminal 1:**

```bash
./build/gcs --allow-localhost &
```

**Terminal 2:**

```bash
./build/autopilot &
```

Mentöre de: “İki ayrı program şu an birbirleriyle konuşuyor.”

---

## 5. Canlı demo — Testler ve ne anlama geliyor?

Her testten sonra ekrandaki **PASS** satırını göster.

### Test 1 — Komutlar

```bash
python3 tests/test_tcp_commands.py
```

Çıktı: `PASS: TCP — received exactly 100 ACKs for 100 commands`

**Söyle:** “100 komut gönderdik, 100 onay cevabı geldi. Komut hattı düzgün çalışıyor.”

---

### Test 2 — Bozuk veri

```bash
python3 tests/test_udp_checksum.py
```

Çıktı: `PASS: UDP checksum — 50 corrupted packets dropped correctly`

**Söyle:** “Yarısı doğru, yarısı bilerek bozuk paket gönderdik. GCS sadece bozukları saydı — 50. Veri kontrolü var.”

---

### Test 3 — Bağlantı kopunca güvenli mod

```bash
python3 tests/test_heartbeat.py
```

Çıktı örneği: `PASS: Heartbeat — SAFE_MODE in 2964ms (expected 2900–3100ms)`

**Söyle:** “GCS nabız sinyalini kesti. Otopilot yaklaşık 3 saniye sonra güvenli moda geçti. Süre beklediğimiz aralıkta.”

---

### Test 4 — Yetkisiz bağlantı (isteğe bağlı, +1 dk)

GCS’i kapatıp sadece belirli ağa izin vererek yeniden aç:

```bash
pkill -f './build/gcs'
sleep 0.5
./build/gcs --subnet 192.168.2.0/24 &
sleep 0.5
python3 tests/test_acl.py
```

Çıktı örneği: `PASS: ACL — unauthorized connection closed in ...ms`

**Söyle:** “İzin verilmeyen IP hemen reddedildi. Basit bir güvenlik katmanı var.”

---

## 6. CI — GitHub Actions

[Actions sekmesinde yeşil işaretle bitmiş son koşuyu aç.]

**Söyle:** “Kodu her gönderdiğimizde sunucuda otomatik derleniyor, programlar başlatılıyor ve aynı testler çalışıyor. Yerelde gördüğümüz PASS satırları orada da var.”

Üç ana satır yeterli:

- 100 ACK → komut tamam  
- 50 corrupted → bozuk veri sayımı tamam  
- SAFE_MODE ~3 sn → güvenli mod tamam  

[Çok `[REJECT]` logu görürseniz: ACL testi öncesi autopilot hâlâ bağlanmaya çalışıyor; test yine geçer, panik yok.]

---

## 7. Kapanış

Projede iki program, iki iletişim kanalı (TCP + UDP), veri kontrolü, nabız kontrolü ve IP güvenliği var. Dört test bunları doğruluyor; CI da her seferinde tekrar ediyor.

Gerçek bir drone projesinde de benzer yapı kullanılır; biz bunun küçük ama çalışan bir modelini kurduk.

Sorularınız var mı?

---

## Sunum öncesi kontrol listesi

- [ ] Derleme bitti (`build/gcs`, `build/autopilot`)
- [ ] İki terminalde programlar çalışıyor
- [ ] Test 1–3 PASS gösterildi
- [ ] (Varsa) CI ekranı gösterildi
- [ ] Süre 10 dakikayı geçmedi
