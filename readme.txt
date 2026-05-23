Giriş dosyası testleri

test dosyasının içindeyken

1. Varsayılan Arşiv Adı Testi: arşiv dosya adı belirtilmezse, varsayılan olarak a.sau kullanılmalıdır.
../bin/tarsau -b test1.txt test2.txt
Klasörde a.sau isimli dosya oluşmalı

2. Maksimum Dosya Sayısı Testi: Sisteme verilen giriş dosyası sayısının en fazla 32 olabilir.
# 33 adet test dosyası oluşturma
touch sinir_testi{1..33}.txt

# Oluşturulan 33 dosyayı programa parametre olarak geçme
../bin/tarsau -b sinir_testi*.txt -o sinir.sau

# Hepsini silmek için
rm sinir_testi*.txt

3. Hatalı Format (Binary/Uyumsuz Dosya) Testi: Hatalı formatta bir giriş dosyası verildiğinde (Örneğin uyumsuz bir dosya), "[dosya adı] giriş dosyasının formatı uyumsuzdur!" mesajı yazılmalı ve sorunsuz bir şekilde programdan çıkılmalıdır.

#İçi tamamen anlamsız karakterlerle (binary veri) dolu bir dosya oluşturduk.
head -c 100 /dev/urandom > t7.dat

#Daha önce oluşturduğumuz çalışan metin dosyalarının yanına bu bozuk t7.dat dosyasını da verip arşivlemeyi deniyoruz.
../bin/tarsau -b test1.txt test2.txt t7.dat -o hata_testi.sau

Beklenen Çıktı
't7.dat' girdi dosyasinin formati uyumsuzdur! Çıktısını bekliyoruz

4. 200 Mb Sınırı Testi

#200 Mb lık metin dosyası oluşturma
yes "Test verisi" | head -c 225000000 > buyuk_dosya1.txt

../bin/tarsau -b buyuk_dosya.txt -o boyut_testi.sau

Beklenen Çıktı
Hata: Toplam girdi dosyasi boyutu 200MB'i asiyor.


tarsau -a daki testler

İzinlerin korunup korunmadığını görebilmek için sıradan dosyalar yerine, farklı izinlere sahip iki dosya oluşturup bunları arşivleyeceğiz.
echo "Birinci dosya" > ozel_test1.txt
echo "Ikinci dosya" > ozel_test2.txt

# İzinleri ayarla
chmod 600 ozel_test1.txt
chmod 755 ozel_test2.txt

# Arşivlemeyi tekrar dene
../bin/tarsau -b ozel_test1.txt ozel_test2.txt -o test_arsiv.sau

1. Hatalı Uzantı Kontrolü: İlk parametrenin .sau ile bitmesi gerekiyor

../bin/tarsau -a ozel_test1.txt
Beklenen Çıktı:Arsiv dosyasi uygunsuz veya bozuk!

2. İkinci Parametre Olmadan (Geçerli Dizine) Çıkarma: Eğer dizin adı girilmezse, dosyaların doğrudan bulunulan klasöre çıkarılması gerekiyor.

# Orijinal dosyaları sil
rm ozel_test1.txt ozel_test2.txt

# Arşivi doğrudan bulunduğun dizine çıkar
../bin/tarsau -a test_arsiv.sau

Dizin belirtilmediğimiz için ozel_test1.txt ve ozel_test2.txt dosyaları bulunduğumuz test klasörüne geri gelmeli.


3. Olmayan Bir Dizine Çıkarma ve İzin Kontrolü:Hem olmayan bir klasör adını verip programın o klasörü yaratmasını bekleyeceğiz, hem de çıkarılan dosyaların az önce verdiğimiz 600 ve 755 izinlerini koruyup korumadığına bakacağız.

# 'yeni_dizin' adında olmayan bir klasöre çıkar
../bin/tarsau -a test_arsiv.sau yeni_dizin

# Çıkarılan dosyaların izinlerini kontrol et
ls -l yeni_dizin/

Beklenen: 1. yeni_dizin adında bir klasör sorunsuz oluşturulmalı.
2. ls -l çıktısında ozel_test1.txt dosyası rw------- (600), ozel_test2.txt dosyası ise rwxr-xr-x (755) izinlerine sahip olmalı.

Dikkat: Eğer izin ikisinde de 755 ise muhtemelen proje windows NTFS dosya sistemi üzerinde çalışıypr. Porjeyi Linuc dosya sistemine taşıyın. Test yaparken bir sorunla karşılaşmadık.

