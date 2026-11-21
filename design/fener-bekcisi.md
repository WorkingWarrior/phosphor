# Fener Bekçisi — tasarım spec'i (Play menüsü, G&W tarzı özgün oyun)

Tek cümle: kayalara gelen gemilere doğru anda ışık tut; hepsi aynı hızda yürür, zorluk hız + kalabalıktan gelir,
kural gereği "imkânsız an" hiç oluşmaz (Monte Carlo ile ölçüldü, aşağıda).

Özgünlük: hiçbir Nintendo oyununun karakteri, çizimi, sahnesi kopyalanmaz. G&W'den sadece tür kuralları alınır
(sabit segment, hayalet çizim, tick hareketi, Game A/B, 3 kaçırma, 999 tavan, bip). İsim, sahne, sprite'lar bizim.

## Döngüler
- Çekirdek (1-3 sn): gemi yaklaşır → şeridine dokun → ışık → gemi döner, +1 / geç kaldın → çarpar, kaçırma.
- Oturum (3-15 dk): hızlanan tempo, 300'de kaçırma silinir (nefes alma anı), 3 kaçırma = bitti → hi-score'a bak → "bir el daha".
- Meta: Game A ve Game B ayrı hi-score (NVS). Ödül/seri/bildirim yok (proje kuralı).

## Ekran yerleşimi (640x172, sabit segmentler)
```
y 0-22   HUD: sol "GAME A"/"GAME B" · orta HI 123 · sağ skor 3 hane (7-seg) + 3 kaçırma ikonu (can simidi)
y 24-68  uzak şerit (L0)   ┐  gemi sprite boyu: uzak ~22px, orta ~30px, yakın ~40px (perspektif)
y 70-116 orta şerit (L1)   ├  her şerit 5 gemi pozisyonu: P5 (doğma, sağ) … P1 (tehlike, kayanın önü)
y118-170 yakın şerit (L2)  ┘
x 0-104    fener kulesi (gövde sabit), 3 pencere: L0 y≈40, L1 y≈92, L2 y≈142 (bekçi + lamba her pencerede ayrı segment)
x 106-150  kayalar (her şeritte), çarpma segmenti P0 burada
x 120-640  ışık huzmesi: her şerit için tek segment (pencereden P1'e uzanan koni), sadece bekçinin şeridi yanar
P1..P5 x merkezleri: 190, 300, 410, 520, 615 (tüm şeritlerde aynı; uzak şerit sprite'ı küçük)
```
Segment listesi (hepsi her karede çizilir: yanmayan = hayalet/GHOST, yanan = LIT):
- 15 gemi (3 şerit × P1..P5), 3 "dönen gemi" (P1'de rotası sağ-yukarı dönmüş poz), 3 batık/çarpma (P0)
- 3 bekçi, 3 lamba, 3 huzme · 3 kaçırma ikonu · skor 3 × 7-seg, HI 3 × 7-seg küçük
- Dalga süsü: her şeritte 2 sabit dalga segmenti, tick'te dönüşümlü yanar (canlılık, oyun etkisi yok)

## Kontrol (karar: dokunduğun şerit)
- Dokun = bekçi anında o şeridin penceresine geçer (yukarı/aşağı adım YOK).
  Gerekçe: (1) Phosphor jest dili "dokun = dokunduğun yere etki"; (2) dokunmatikte basılacak tuş hissi yok,
  doğrudan hedef yanlış dokunmayı azaltır; (3) şerit bandı 44-52 px, parmak (~40 px) sığar; (4) adil kural (aşağıda)
  hareket süresini değil tepki süresini ölçüyor, adım sayma ek zorluk değil sadece sinir bozucu olurdu.
- Basılı tut 1 sn = duraklat (oyun sırasında). Duraklatma ekranında ve başlık ekranında sağ/sol kaydır = Play menüsüne dön.
- Oyun sırasında kaydırma YOK SAYILIR (şerit değiştirirken kayan parmak sayfa değiştirmesin). Dokunma = basma anı
  (touch down), bırakmayı bekleme.
- Ekran 30 sn dokunulmazsa otomatik duraklat.

## Kurallar
- Her tick'te tüm gemiler 1 pozisyon sola: P5→P4→…→P1→çözüm. Doğan gemi P5'te görünür (5 tick yolu).
- Çözüm: P1'deki gemi bir sonraki tick'te ilerlerken bekçi o şeritteyse KURTULDU (o tick "dönen gemi" yanar, +1 puan,
  bip), değilse ÇARPTI (P0 batık segmenti yanar, kaçırma +1).
- Çarpmada oyun 1.2 sn donar (batık yanıp söner, kaçırma sesi), sonra devam. Ekranda kalan gemiler yerinde kalır.
- 3. kaçırma = OYUN BİTTİ: batık + tüm segmentler 3 kez yanıp söner, ses; skor > HI ise HI yanıp söner, NVS'e yazılır
  (`fener_hiA`, `fener_hiB`). 2 sn sonra başlık ekranı.
- 300 ve 700 puanda kaçırmalar silinir (kısa arpej). O anda hiç kaçırma yoksa sonraki 20 kurtarma 2 puan (skor yanıp söner).
- 999'dan sonra 000'a döner, oyun sürer; hız eğrisi en az 500 seviyesinden devam eder.

## Adil doğma kuralı (imkânsız an yok)
Her tick en fazla 1 doğma → iki gemi asla aynı tick'te çözüme gelmez (hepsi aynı hızda).
Ek kurallar:
1. Şerit P5 ve P4'ü boşsa doğabilir (aynı şeritte en az 1 boş pozisyon arası).
2. Tick < 400 ms iken: önceki tick'te doğma olduysa bu tick sadece AYNI şeritte doğabilir.
   Böylece farklı şeritte art arda iki çözüm arası her zaman ≥ ~400 ms (hızlı insan şerit değiştirme ~280±70 ms).
3. Ekrandaki gemi sınırı: Game A 3 (500+ puanda 4) · Game B 4 (500+ puanda 5).
4. Doğma olasılığı (tick başına): A = 0.38 + 0.02 × yüzler · B = 0.48 + 0.02 × yüzler. Şerit = uygun şeritlerden rastgele.

## Hız eğrisi (tick ms)
`v = skor % 1000; h = v / 100; w = v % 100` (1000'i geçince `h = max(h, 5)`)
- Game A: `tick = max(320, 650 - 30*h - 0.9*w)` → 0: 650 · 500: 500 · 999: 320
- Game B: `tick = max(260, 520 - 25*h - 0.7*w)` → 0: 520 · 500: 395 · 999: 260
Her yüzlüğün başında ~60 ms rahatlama olur (w sıfırlanır): G&W'deki "tempo nefesi".

## Ölçüm (Monte Carlo, `design/fener-bekcisi-mc.py`, tohum 7, `python3 design/fener-bekcisi-mc.py`)
| | aynı anda çözüm | farklı şeritte en kısa ara | ekrandaki ort. gemi | skor p25 / medyan / p75 | medyan süre |
|---|---|---|---|---|---|
| Game A | 0 (300 koşu, 999+) | 396 ms | 2.1 | 239 / 501 / 615 | 13 dk |
| Game B | 0 | 397 ms | 2.7 | 156 / 225 / 294 | 4 dk |
İnsan modeli TAHMİN: şerit değiştirme N(280, 70) ms, dikkat kaçırma %0.15 + %0.4 × ekrandaki gemi.
Gerçek değerler cihazda playtest ile ayarlanır (aşağıda). Tüm sayılar tek yerde sabit tablo olarak dursun (`FenerTuning`).

## Sesler (Radio_Beep; radyo çalıyorsa oyun girişinde radyo duraklatılır, çıkışta devam)
- tick: çok kısa tık (1 kHz, 6 ms) — Ayarlar'daki sesle; "tık sesi" Play içinden kapatılabilir
- kurtarma: 2 kHz 40 ms · çarpma: 800→500→300 Hz, 3 × 90 ms · bonus/silme: 1→1.5→2 kHz arpej
- oyun bitti: 400 Hz 600 ms · başlangıç: 2 kısa bip · yeni HI: 3 hızlı yüksek bip
Radio_Beep imzasını koddan kontrol et; frekans/süre almıyorsa en yakın karşılık.

## Akış
Play menüsü → başlık ekranı (fener sahnesi hayalet çizimle, "GAME A  HI 123" sol yarı, "GAME B  HI 045" sağ yarı;
dokun = o mod başlar) → 2 bip, 1 sn hazır → oyun → (bitti) → başlık. Saat bu ekranda HUD'da göstermez (dikkat dağıtmasın).

## 4 tema
- VFD: mevcut `vfd::GHOST/LIT/HALO` (yeşil-mavi fosfor), hayaletler çok sönük görünür → en "G&W/LCD" hissi bu.
- C64: mavi zemin + açık mavi kenarlık, LIT açık mavi/beyaz, 2x geniş piksel; hayalet çok koyu mavi. Skor C64-style font.
- Amiga: copper şeritli gökyüzü (zemin), segmentler beyaz/turuncu, hayaletler yarı saydam gri; başlık çubuğunda "FENER".
- 2600: blok sprite'lar, hayalet YOK (o makinede LCD yok), huzme düz renk bant; skor 2600 rakamları.
Sprite'lar string sprite (`px::sprite` biçimi), tema paletinden renklenir; telifli çizim yok.

## Test planı (cihazda)
1. Şerit bantlarına dokunma isabeti: 20 hızlı dokunuşta yanlış şerit sayısı (hedef 0-1).
2. Game B 999 tempoda (260 ms) 2 dk oyna: "haksız öldüm" anı var mı? Varsa F eşiğini (400) yükselt.
3. İlk oyun medyanı: 3 kişi × 3 el, Game A skorları; 100'ün altındaysa başlangıç tick'ini +50 ms.
4. Tık sesi 5 dk sonra rahatsız ediyor mu → varsayılan kapalı mı açık mı karar.
