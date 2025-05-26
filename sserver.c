/* ascii_streaming_server - sserver.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

// Kanal sayısı ve client sınırları
#define MAX_CHANNELS 3
#define MAX_CLIENTS_PER_CHANNEL 10

// Frame ve buffer büyüklükleri
#define FRAME_SIZE 2048
#define BUFFER_SIZE 100
#define PORT 12345

// Kontrol komutları, proje kapsamında yok + 
#define CMD_PLAY     1
#define CMD_PAUSE    2
#define CMD_RESUME   3
#define CMD_RESTART  4
#define CMD_X2       5
#define CMD_X4       6

// Bir frame yapısı: veriyi ve kaç kişi izleyecekse onu tutar
typedef struct {
    char data[FRAME_SIZE];
    int refcount;
} FrameEntry;

// Thread-safe buffer yapısı
typedef struct {
    FrameEntry frames[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    sem_t full;
    sem_t empty;
} FrameBuffer;

// Her kanalın durumu
typedef struct {
    FrameBuffer buffer;
    FILE *video;
    int client_sockets[MAX_CLIENTS_PER_CHANNEL];
    int client_count;
    pthread_mutex_t client_lock;
    int control_command;
    int is_playing;
    pthread_mutex_t control_lock;
} Channel;

// Kanal listesi
Channel channels[MAX_CHANNELS];
int total_channels = 0;

// Komut satırı argümanlarını parse et
void parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        // -s <kanal_sayısı>
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            total_channels = atoi(argv[++i]);
            if (total_channels < 1 || total_channels > 3) {
                fprintf(stderr, "Error: Invalid channel count\n");
                exit(EXIT_FAILURE);
            }
        // -ch1 <dosya>
        } else if (strncmp(argv[i], "-ch", 3) == 0 && i + 1 < argc) {
            int ch = argv[i][3] - '1';
            channels[ch].video = fopen(argv[++i], "r");
            if (!channels[ch].video) {
                perror("Error opening video file");
                exit(EXIT_FAILURE);
            } else {
                printf("Video dosyası açıldı: Kanal %d\n", ch + 1);
            }
        }
    }
}

// Frame üreten thread (her kanal için ayrı)
void *producer_thread(void *arg) {
    int ch = *(int *)arg;
    char frame[FRAME_SIZE];
    while (1) {
        FrameBuffer *buf = &channels[ch].buffer;
        while (!feof(channels[ch].video)) {
            frame[0] = '\0';
            int line_count = 0;
            //  Her frame yaklaşık 22 satır ASCII veriden oluşuyor
            while (line_count < 22) {
                char line[256];
                if (fgets(line, sizeof(line), channels[ch].video)) {
                    strncat(frame, line, sizeof(frame) - strlen(frame) - 1);
                } else {
                    strncat(frame, "\n", sizeof(frame) - strlen(frame) - 1);
                }
                line_count++;
            }

            // Buffer boş slot bekle
            sem_wait(&buf->empty);
            pthread_mutex_lock(&buf->lock);

            // Frame'i buffer'a yaz
            strncpy(buf->frames[buf->tail].data, frame, FRAME_SIZE);
            buf->frames[buf->tail].refcount = channels[ch].client_count;
            buf->tail = (buf->tail + 1) % BUFFER_SIZE;
            buf->count++;

            pthread_mutex_unlock(&buf->lock);
            sem_post(&buf->full);
        }

        //  Video bittiğinde başa sar
        fseek(channels[ch].video, 0, SEEK_SET);
        clearerr(channels[ch].video);
    }
    return NULL;
}

// Tüm client'lara frame gönderme fonksiyonu
void send_to_clients(int ch) {
    FrameBuffer *buf = &channels[ch].buffer;
    pthread_mutex_lock(&buf->lock);
    int index = buf->head;
    if (buf->count == 0 || buf->frames[index].refcount <= 0) {
        pthread_mutex_unlock(&buf->lock);
        return;
    }
    char *frame = buf->frames[index].data;
    int len = strlen(frame);
    pthread_mutex_unlock(&buf->lock);

    pthread_mutex_lock(&channels[ch].client_lock);
    for (int i = 0; i < channels[ch].client_count;) {
        int client_fd = channels[ch].client_sockets[i];
        char header[16];
        sprintf(header, "%04d", len);

        // Frame boyutu ve verisi gönderiliyor
        ssize_t sent1 = send(client_fd, header, 4, 0);
        ssize_t sent2 = send(client_fd, frame, len, 0);

        //  Eğer gönderim başarısızsa client'ı çıkar
        if (sent1 < 0 || sent2 < 0) {
            close(client_fd);
            channels[ch].client_sockets[i] = channels[ch].client_sockets[--channels[ch].client_count];
            continue;
        }

        pthread_mutex_lock(&buf->lock);
        buf->frames[index].refcount--;
        if (buf->frames[index].refcount == 0) {
            buf->head = (buf->head + 1) % BUFFER_SIZE;
            buf->count--;
            sem_post(&buf->empty);
        }
        pthread_mutex_unlock(&buf->lock);
        i++;
    }
    pthread_mutex_unlock(&channels[ch].client_lock);
}

// Kanal işleyici: frame gönderme zamanlamasını kontrol eder
void *channel_worker(void *arg) {
    int ch = *(int *)arg;
    int delay_us = 50000;
    while (1) {
        pthread_mutex_lock(&channels[ch].control_lock);
        int cmd = channels[ch].control_command;
        int is_playing = channels[ch].is_playing;
        pthread_mutex_unlock(&channels[ch].control_lock);

        if (!is_playing) {
            usleep(100000);
            continue;
        }

        switch (cmd) {
            case CMD_X2: delay_us = 25000; break;
            case CMD_X4: delay_us = 12500; break;
            default:     delay_us = 50000; break;
        }

        FrameBuffer *buf = &channels[ch].buffer;
        sem_wait(&buf->full);
        send_to_clients(ch);
        usleep(delay_us);
    }
    return NULL;
}

// Her client için çalıştırılan thread
void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    FILE *fp = fdopen(client_socket, "r+");
    if (!fp) {
        perror("fdopen");
        close(client_socket);
        pthread_exit(NULL);
    }

    char buf[32];

    // İlk olarak kanal numarasını al
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        pthread_exit(NULL);
    }

    buf[strcspn(buf, "\r\n")] = '\0';
    int ch = atoi(buf);
    if (ch < 1 || ch > total_channels) {
        fclose(fp);
        pthread_exit(NULL);
    }
    ch--;

    //  Client kanalına ekleniyor
    pthread_mutex_lock(&channels[ch].client_lock);
    if (channels[ch].client_count < MAX_CLIENTS_PER_CHANNEL) {
        channels[ch].client_sockets[channels[ch].client_count++] = client_socket;
        printf("[INFO] Kanal %d: yeni bir istemci bağlandı.\n", ch + 1);
    } else {
        pthread_mutex_unlock(&channels[ch].client_lock);
        fclose(fp);
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&channels[ch].client_lock);

    // Client'tan gelen komutları sürekli dinle
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        int cmd = atoi(buf);

        pthread_mutex_lock(&channels[ch].control_lock);
        printf("[INFO] Kanal %d - gelen komut: '%s' (%d)\n", ch + 1, buf, cmd);
        switch (cmd) {
            case CMD_PLAY:
            case CMD_RESUME:
                channels[ch].is_playing = 1;
                channels[ch].control_command = CMD_PLAY;
                break;
            case CMD_PAUSE:
                channels[ch].is_playing = 0;
                channels[ch].control_command = CMD_PAUSE;

                // Buffer sıfırlanıyor
                pthread_mutex_lock(&channels[ch].buffer.lock);
                channels[ch].buffer.head = 0;
                channels[ch].buffer.tail = 0;
                channels[ch].buffer.count = 0;

                // Semaforlar yeniden ayarlanıyor
                int val;
                while (sem_trywait(&channels[ch].buffer.full) == 0);
                sem_getvalue(&channels[ch].buffer.empty, &val);
                for (int i = val; i < BUFFER_SIZE; i++) sem_post(&channels[ch].buffer.empty);
                pthread_mutex_unlock(&channels[ch].buffer.lock);

                printf("[INFO] Kanal %d: PAUSE → Buffer temizlendi\n", ch + 1);
                break;
            case CMD_RESTART:
                fseek(channels[ch].video, 0, SEEK_SET);
                clearerr(channels[ch].video);
                break;
            case CMD_X2:
                channels[ch].is_playing = 1;
                channels[ch].control_command = CMD_X2;
                break;
            case CMD_X4:
                channels[ch].is_playing = 1;
                channels[ch].control_command = CMD_X4;
                break;
            default:
                break;
        }
        pthread_mutex_unlock(&channels[ch].control_lock);
    }

    fclose(fp);
    pthread_exit(NULL);
}

//  Ana fonksiyon: sunucuyu başlatır, thread'leri başlatır
int main(int argc, char *argv[]) {
    parse_args(argc, argv);

    // Tüm kanalların başlangıç ayarları
    for (int i = 0; i < total_channels; i++) {
        FrameBuffer *buf = &channels[i].buffer;
        buf->head = buf->tail = buf->count = 0;
        pthread_mutex_init(&buf->lock, NULL);
        sem_init(&buf->full, 0, 0);
        sem_init(&buf->empty, 0, BUFFER_SIZE);
        pthread_mutex_init(&channels[i].client_lock, NULL);
        pthread_mutex_init(&channels[i].control_lock, NULL);
        channels[i].control_command = CMD_PLAY;
        channels[i].is_playing = 1;
    }

    // Kanal başına producer ve worker thread'lerini başlat
    pthread_t prod_threads[MAX_CHANNELS], chan_threads[MAX_CHANNELS];
    for (int i = 0; i < total_channels; i++) {
        int *arg1 = malloc(sizeof(int));
        int *arg2 = malloc(sizeof(int));
        *arg1 = *arg2 = i;
        pthread_create(&prod_threads[i], NULL, producer_thread, arg1);
        pthread_create(&chan_threads[i], NULL, channel_worker, arg2);
    }

    // TCP sunucu kurulumu
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    printf("Sunucu başlatıldı, bağlantılar dinleniyor...\n");

    // Yeni client bağlantılarını dinle
    while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen))) {
        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_create(&tid, NULL, client_handler, pclient);
    }

    return 0;
}
