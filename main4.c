#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>

#define M 5 // Количество рядов
#define N 10 // Количество шкафов в ряду
#define K 20 // Количество книг в шкафу

#define SEM_NAME "/book_sem" // Имя семафора
#define SHM_NAME "/book_shm" // Имя разделяемой памяти

typedef struct {
    char id[32]; // Идентификатор книги (номер или название)
    int row; // Номер ряда
    int shelf; // Номер шкафа
    int book; // Номер книги в шкафу
} Book;

Book *catalog; // Указатель на разделяемую память
sem_t *sem; // Указатель на семафор

void sig_handler(int signum) {
    // Обработчик сигнала
    switch (signum) {
        case SIGINT:
        case SIGTERM:
            // Удаление семафора и разделяемой памяти
            sem_close(sem);
            sem_unlink(SEM_NAME);
            munmap(catalog, sizeof(Book) * M * N * K);
            shm_unlink(SHM_NAME);
            exit(EXIT_SUCCESS);
        default:
            break;
    }
}

int main() {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    int fd_shm; // Файловый дескриптор разделяемой памяти
    int i, j, k;

    // Создание и инициализация семафора
    sem = sem_open(SEM_NAME, O_CREAT, 0644, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // Создание и отображение разделяемой памяти
    fd_shm = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
    if (fd_shm == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(fd_shm, sizeof(Book) * M * N * K) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }
    catalog = mmap(NULL, sizeof(Book) * M * N * K, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (catalog == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Создание дочерних процессов для составления подкаталога
    for (i = 0; i < M; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Дочерний процесс
            // Составление подкаталога для ряда i
            for (j = 0; j < N; j++) {
                for (k = 0; k < K; k++) {
                    Book *book = &catalog[i * N * K + j * K + k];
                    sprintf(book->id, "Book %d-%d-%d", i, j, k);
                    book->row = i;
                    book->shelf = j;
                    book->book = k;
                }
            }
            sleep(rand() % 15);
            sem_post(sem); // Отправка сигнала библиотекарю
            exit(EXIT_SUCCESS); // Завершение работы дочернего процесса
        }
    }
    // Ожидание завершения дочерних процессов и сортировка каталога
    for (i = 0; i < M; i++) {
        sem_wait(sem); // Ожидание сигнала от дочернего процесса
    }
    for (i = 0; i < M * N * K - 1; i++) { // Сортировка каталога
        for (j = 0; j < M * N * K - i - 1; j++) {
            if (strcmp(catalog[j].id, catalog[j + 1].id) > 0) {
                Book temp = catalog[j];
                catalog[j] = catalog[j + 1];
                catalog[j + 1] = temp;
            }
        }
    }

// Вывод каталога на экран
    printf("Catalog:\n");
    for (i = 0; i < M * N * K; i++) {
        printf("%s, row=%d, shelf=%d, book=%d\n", catalog[i].id, catalog[i].row, catalog[i].shelf, catalog[i].book);
    }

// Удаление семафора и разделяемой памяти
    sem_close(sem);
    sem_unlink(SEM_NAME);
    munmap(catalog, sizeof(Book) * M * N * K);
    shm_unlink(SHM_NAME);

    return 0;
}