#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
struct dirent *entry;

int main(char *roomname, char *username){

char path[256];
snprintf(path, sizeof(path), "/tmp/chatroom-%s", roomname);
mkdir(path, 0777); // Varsa hata verir ama sorun değil

char user_pipe[512];
snprintf(user_pipe, sizeof(user_pipe), "%s/%s", path, username);
mkfifo(user_pipe, 0666);

pid_t pid = fork();

if (pid == 0){

    int fd = open(user_pipe, O_RDONLY);
    char buf[1024];
    while(read(fd, buf, sizeof(buf)) > 0) {
        printf("%s", buf);
    }
} else {
    // YAZMA DÖNGÜSÜ (Parent)
    char *arr[80];
    fgets(arr, sizeof(arr) ,stdin);
    char *dir = opendirf("/tmp/chatroom-%s", roomname);
    
    while(entry = readdir(dir)){
        if (fork() == 0) {
        char target_path[1024];
        snprintf(target_path, sizeof(target_path), "%s/%s", dir, entry->d_name);
        int fd_target = open(target_path, O_WRONLY);
        write(fd_target, arr, strlen(arr));
        close(fd_target);
        exit(0); // Yazma bitince child ölsün
    }
    closedir(dir);
    }
    // 2. opendir("/tmp/chatroom-roomname") ile klasörü aç
    // 3. readdir() ile tüm kullanıcı borularını bul
    // 4. Her kullanıcı için fork() yapıp borusuna yaz (Broadcast)
}



}