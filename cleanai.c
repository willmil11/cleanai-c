#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "libs/cJSON.h"
#include "libs/cJSON.c"

#include "libs/miniz.h"
#include "libs/miniz.c"

#ifdef _WIN32 //windows compability is pain ;(
#include <windows.h>
#include <conio.h>
#include <tlhelp32.h>

unsigned long getPid(){
    return (unsigned long)GetCurrentProcessId();
}

unsigned long getParentPid(){
    DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (!Process32First(snap, &pe)) { CloseHandle(snap); return 0; }

    DWORD ppid = 0;
    do {
        if (pe.th32ProcessID == pid) { ppid = pe.th32ParentProcessID; break; }
    } while (Process32Next(snap, &pe));

    CloseHandle(snap);
    return (unsigned long)ppid;
}

long long time_ms(){
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (long long)(uli.QuadPart / 10000);
}

char* input_with_timeout(char* qry, int timeout_ms){
    printf("%s", qry);
    fflush(stdout);
    
    char* buff = malloc(4096);
    if (!buff) {
        printf("Failed to allocate memory to read input.\n");
        return NULL;
    }
    
    int pos = 0;
    long long start_time = time_ms();
    
    while (pos < 4095) {
        if (_kbhit()) {
            int k = _getch();
            if (k == 0 || k == 0xE0) { (void)_getch(); continue; } // swallow special keys
            char c = (char)k;
            
            if (c == '\r' || c == '\n') {
                putchar('\n');
                buff[pos] = '\0';
                return buff;
            } else if (c == '\b' && pos > 0) {
                printf("\b \b");
                pos--;
            } else if (c >= 32 && c <= 126) {
                putchar(c);
                buff[pos++] = c;
            }
        }
        
        if (time_ms() - start_time >= timeout_ms) {
            free(buff);
            return NULL;
        }
        
        Sleep(1);
    }
    
    buff[pos] = '\0';
    return buff;
}

void* smalloc(size_t size, const char* sharename){
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        (DWORD)((unsigned long long)size >> 32),
        (DWORD)(size & 0xFFFFFFFF),
        sharename
    );
    if (!hMap) return NULL;

    void* p = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    CloseHandle(hMap); // view keeps mapping alive
    return p;
}

void* rmalloc(const char* sharename){
    HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, sharename);
    if (!hMap) return NULL;

    /* 0 size maps the entire section on Windows */
    void* p = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CloseHandle(hMap);
    return p;
}

#else
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

unsigned long getPid(){
    return (unsigned long)getpid();
}
unsigned long getParentPid(){
    return (unsigned long)getppid();
}

long long time_ms(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
}

char* input_with_timeout(char* qry, int timeout_ms){
    printf("%s", qry);
    fflush(stdout);
    
    char* buff = malloc(4096);
    if (!buff) {
        printf("Failed to allocate memory to read input.\n");
        return NULL;
    }
    
    fd_set readfds;
    struct timeval tv;
    long long start_time = time_ms();
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        long long elapsed = time_ms() - start_time;
        if (elapsed >= timeout_ms) {
            break;
        }
        
        long long remaining = timeout_ms - elapsed;
        tv.tv_sec = remaining / 1000;
        tv.tv_usec = (remaining % 1000) * 1000;
        
        int result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        
        if (result > 0) {
            if (fgets(buff, 4096, stdin)) {
                size_t len = strlen(buff);
                if (len > 0 && buff[len - 1] == '\n') {
                    buff[len - 1] = '\0';
                }
                return buff;
            }
        } else if (result == 0) {
            break;
        } else {
            break;
        }
    }
    
    free(buff);
    return NULL;
}

void* smalloc(size_t size, const char* sharename){
    int fd = shm_open(sharename, O_CREAT | O_RDWR, 0666);
    if (fd == -1) return NULL;

    if (ftruncate(fd, (off_t)size) == -1) {
        close(fd);
        shm_unlink(sharename);
        return NULL;
    }

    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) {
        shm_unlink(sharename);
        return NULL;
    }
    return p;
}

void* rmalloc(const char* sharename){
    int fd = shm_open(sharename, O_RDWR, 0666);
    if (fd == -1) return NULL;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return NULL;
    }
    size_t size = (size_t)st.st_size;

    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return NULL;
    return p;
}
#endif

float sqrtf(float to){
    return __builtin_sqrtf(to);
}

char* input(char* qry){
    printf("%s", qry);
    fflush(stdout);
    char* buff = malloc(4096); //4kb is sufficient
    if (!buff){
        printf("Failed to allocate memory to read input.\n");
        return NULL;
    }
    if (fgets(buff, 4096, stdin)){
        size_t len = strlen(buff);
        if (len > 0 && buff[len - 1] == '\n') {
            buff[len - 1] = '\0'; //remove the newline
        }
        return buff;
    }
    else{
        free(buff);
        return NULL;
    }
}

void help(char* issue){
    if (issue == NULL){
        issue = "No args found.";
    }
    char* repeat(char item, int count){
        char* buff = malloc(count + 1);
        if (!buff){
            printf("Failed to allocate memory to repeat text.\n");
            return NULL;
        }
        for (int index = 0; index < count; index++){
            buff[index] = item;
        }
        buff[count] = '\0';
        return buff;
    }
    char* rep = repeat('=', strlen(issue));
    if (!rep){
        return;
    }
    printf("=====%s=====\n", rep);
    printf("==== %s ====\n", issue);
    printf("=====%s=====\n", rep);
    free(rep);
    printf("\n");
    printf("cleanai --new\n");
    printf("             --config path/to/config.json\n");
    printf("                                         --train\n");
    printf("                                                   [--pretrain]\n");
    printf("                                         --pretrain\n");
    printf("                                                   [--train]\n");
    printf("        --load path/to/model.zip\n");
    printf("                                [--config path/to/config.json]\n");
    printf("                                                              [--train]\n");
    printf("                                                                          [--pretrain]\n");
    printf("                                                              [--pretrain]\n");
    printf("                                                                          [--train]\n");
    printf("\n");
    printf("Note: Arguments between square brackets ([...]) are optional.\n");
}

bool file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("Failed to open file at path \"%s\".\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("Failed to allocate memory to read file at path \"%s\".\n", path);
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, size, f);
    fclose(f);

    if (read != size) {
        printf("Failed to read file at path \"%s\".\n", path);
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0'; // null-terminate for safety
    return buffer;
}

int main(int argc, char** argv){
    int* ids = malloc(1); //1 byte init alloc

    if (!ids){
        printf("Failed memory allocation for ids, ids will not be tracked therefore ids may not be strictly unique.\n");
    }

    int ids_len = 0;

    srand(time(NULL));

    int genid(){
        int id;
        while (true){
            id = rand() % 100000;

            if (!ids){
                return id;
            }

            if (ids_len == 100000){
                printf("All possible ids combinations exhaused, this id will be a duplicate of another id.\n");
                return id;
            }

            bool found = false;
            for (int index = 0; index < ids_len; index++){
                if (ids[index] == id){
                    found = true;
                    break;
                }
            }

            if (!found){
                break;
            }
        }
        //grw
        int* tmp_ids = realloc(ids, (ids_len + 1) * sizeof(int));
        if (!tmp_ids){
            printf("Failed memory allocation to grow ids list.\n");
            return id; //better to return something than nothing
        }
        ids = tmp_ids;

        ids[ids_len] = id;
        ids_len++;
        return id;
    }

    int freeId(int id){
        int indexOfId = -1;
        for (int index = 0; index < ids_len; index++){
            if (id == ids[index]){
                indexOfId = index;
                break;
            }
        }
        if (indexOfId == -1){
            return -1; //id not found
        }

        //shrnk
        int* n_ids = malloc((ids_len - 1) * sizeof(int));
        if (!n_ids){
            printf("Failed to allocate memory to shrink ids list.\n");
            return -1;
        }

        int offset = 0;
        for (int index = 0; index < ids_len; index++){
            if (index == indexOfId){
                offset += 1;
                continue;
            }
            n_ids[index - offset] = ids[index];
        }

        free(ids);
        ids = n_ids;
        ids_len--;
        return 0;
    }

    long long** timers = malloc(1); //1 byte init alloc
    
    if (!timers){
        printf("Failed memory allocation for timers, timers will not be available.\n");
    }

    int timers_len = 0; //0 elements

    long long timer(){
        if (!timers){
            return -1;
        }

        long long curr_time = time_ms();
        
        //grw
        long long id = (long long)(genid());
        
        long long** tmp_timers = realloc(timers, (timers_len + 1) * sizeof(long long*));
        if (!tmp_timers){
            printf("Failed memory allocation to grow timers list.\n");
            return -1;
        }
        timers = tmp_timers;
        timers[timers_len] = malloc(2 * sizeof(long long)); //id, timestamp

        if (!timers[timers_len]){
            printf("Failed memory allocation to grow timers list.\n");
        }
        timers[timers_len][0] = id;
        timers[timers_len][1] = curr_time;
        timers_len++;
        return id;
    }

    long long timer_end(long long timer_id){
        int indexOfTimer = -1;
        long long timer_time;
        for (int index = 0; index < timers_len; index++){
            if (timers[index][0] == timer_id){
                indexOfTimer = index;
                timer_time = time_ms() - timers[index][1];
                break;
            }
        }
        if (indexOfTimer == -1){
            return -1; //invalid timer id.
        }

        //shrnk
        long long** new_timers = malloc((timers_len - 1) * sizeof(long long*));
        if (!new_timers){
            printf("Failed memory allocation to shrink timers list.\n");
            return -1;
        }

        int offset = 0;
        for (int index = 0; index < timers_len; index++){
            if (index == indexOfTimer){
                offset++;
                continue;
            }
            new_timers[index - offset] = malloc(2 * sizeof(long long));
            if (!new_timers[index - offset]){
                printf("Failed memory allocation to shrink timers list.\n");
                for (int subindex = 0; subindex < timers_len - index - offset; subindex++){
                    free(new_timers[subindex]);
                }
                free(new_timers);
                return -1;
            }
            new_timers[index - offset][0] = timers[index][0];
            new_timers[index - offset][1] = timers[index][1];
        }

        for (int index = 0; index < timers_len; index++){
            free(timers[index]);
        }
        free(timers);
        timers = new_timers;
        timers_len--;
        
        //oh also
        freeId((int)(timer_id));

        return timer_time;
    }

    //store filename separately
    char* self_filename = malloc(strlen(argv[0]) + 1);
    if (!self_filename){
        printf("Failed to allocate memory to parse args.\n");
        return 1;
    }
    strcpy(self_filename, argv[0]);
    //remove file name from argv
    char** nargv = malloc((argc + 1) * sizeof(char*));
    if (!nargv){
        printf("Failed to allocate memory to parse args.\n");
        return 1;
    }
    nargv[argc] = NULL;
    for (int index = 1; index < argc; index++){
        nargv[index - 1] = malloc(strlen(argv[index]) + 1);
        if (!nargv[index - 1]){
            printf("Failed to allocate memory to parse args.\n");
            return 1; //os will auto reclaim memory
        }
        strcpy(nargv[index - 1], argv[index]);
    }
    argv = nargv;
    argc--;

    //parse arguments
    bool do_pretrain = false;
    bool do_train = false;
    bool new = false;
    bool load = false;

    char* valid_flags[] = {"--new", "--load", "--config", "--train", "--pretrain", NULL};
    int valid_flags_len = 0;
    while (true){
        if (!(valid_flags[valid_flags_len] == NULL)){
            valid_flags_len++;
        }
        else{
            break;
        }
    }
    
    char* config_location = malloc(1); //dummy init
    bool config_init = false;
    char* model_location = malloc(1); //dummy init
    if (!config_location){
        printf("Failed to allocate memory to parse args.\n");
        return -1;
    }
    if (!model_location){
        printf("Failed to allocate memory to parse args.\n");
        return -1;
    }

    config_location[0] = '\0'; //if printed safe anyways :)
    model_location[0] = '\0';

    if (argc == 0){
        help(NULL);
        return 0;
    }
    
    bool nextIsVal = false;
    for (int index = 0; index < argc; index++){
        char* arg = argv[index];
        if (nextIsVal){
            nextIsVal = false;
            continue;
        }
        if (strcmp(arg, "--new") == 0){
            if (new){
                help("You can't specify --new multiple times.");
                return 0;
            }
            if (load){
                help("You can't specify --new and --load at the same time.");
                return 0;
            }
            new = true;
        }
        else{
            if (strcmp(arg, "--load") == 0){
                if (load){
                    help("You can't specify --load multiple times.");
                    return 0;
                }
                if (new){
                    help("You can't specify --load and --new at the same time.");
                    return 0;
                }
                load = true;
                if (argc - index - 1 == 0){
                    help("You need to specify a model file path after --load.");
                    return 0;
                }
                nextIsVal = true;
                char* nextArg = argv[index + 1];
                for (int subindex = 0; subindex < valid_flags_len; subindex++){
                    if (strcmp(nextArg, valid_flags[subindex]) == 0){
                        nextIsVal = false;
                        break;
                    }
                }
                if (!nextIsVal){
                    help("You need to specify a model file path after --load.");
                    return 0;
                }
                model_location = realloc(model_location, strlen(nextArg) + 1);
                if (!model_location){ //no need to use tmp we exit if fail anyways
                    printf("Failed to allocate memory to parse args.\n");
                    return 1;
                }
                strcpy(model_location, nextArg);
            }
            else{
                if (strcmp(arg, "--train") == 0){
                    if (do_train){
                        help("You can't specify --train multiple times.");
                        return 0;
                    }
                    do_train = true;
                }
                else{
                    if (strcmp(arg, "--pretrain") == 0){
                        if (do_pretrain){
                            help("You can't specify --pretrain multiple times.");
                            return 0;
                        }
                        do_pretrain = true;
                    }
                    else{
                        if (strcmp(arg, "--config") == 0){
                            if (config_init){
                                help("You can't specify --config multiple times.");
                                return 0;
                            }
                            config_init = true;
                            if (argc - index - 1 == 0){
                                help("You need to specify a config file path after --config.");
                                return 0;
                            }
                            nextIsVal = true;
                            char* nextArg = argv[index + 1];
                            for (int subindex = 0; subindex < valid_flags_len; subindex++){
                                if (strcmp(nextArg, valid_flags[subindex]) == 0){
                                    nextIsVal = false;
                                    break;
                                }
                            }
                            if (!nextIsVal){
                                help("You need to specify a config file path after --config.");
                                return 0;
                            }
                            config_location = realloc(config_location, strlen(nextArg) + 1);
                            if (!config_location){
                                printf("Failed to allocate memory to parse args.\n");
                                return 1;
                            }
                            strcpy(config_location, nextArg);
                        }
                        else{
                            int help_message_len = strlen("Arg \"") + strlen(arg) + strlen("\" is invalid.") + 1;
                            char* help_message = malloc(help_message_len);
                            if (!help_message){
                                printf("Failed to allocate memory to parse args.\n");
                                return 1;
                            }
                            sprintf(help_message, "Arg \"%s\" is invalid.", arg);
                            help(help_message);
                            return 0;
                        }
                    }
                }
            }
        }
    }

    if (new){
        if ((!do_pretrain) && (!do_train)){
            help("You need to specify either or both --pretrain and --train with --new.");
        }
    }

    if (new){
        if (!config_init){
            help("You need to specify a config file path with --config.");
            return 0;
        }
    }

    if (load){
        if (do_train || do_pretrain){
            if (!config_init){
                help("You need to specify a config file path with --config.");
                return 0;
            }
        }
    }

    if (!file_exists(config_location)){
        printf("Failed to open config file. Most likely causes are that it doesn't exist or that you don't have the required permissions to read it.\n");
        return 1;
    }

    if (load){
        if (!file_exists(model_location)){
            printf("Failed to open model file. Most likely causes are that it doesn't exist or that you don't have the required permissions to read it.\n");
            return -1;
        }
    }

    printf("Arguments parsed successfully :)\n");

    printf("Reading config file...\n");
    char* config_file = read_file(config_location);
    if (!config_file){
        printf("Failed to read config file.\n");
        return 1;
    }
    //try to parse json
    cJSON* config = cJSON_Parse(config_file);
    if (!config){
        printf("Failed to parse config. Common cause: corrupted json.\n");
        return 1;
    }

    bool isInt(double n){
        if ((int)(n) == n){
            return true;
        }
        else{
            return false;
        }
    }

    bool isFloat(double n){
        return !isInt(n);
    }

    cJSON* pre_training_paths_raw = cJSON_GetObjectItem(config, "pre-training-paths");
    char** pre_training_paths = NULL;
    if (!cJSON_IsArray(pre_training_paths_raw)){
        if (!do_pretrain){
            printf("[Config] [Warning] pre-training-paths is missing/corrupted but --pretrain is not specified therefore this can be ignored.\n");
        }
        else{
            printf("[Config] [Fatal] pre-training-paths is missing/corrupted.\n");
            return 1;
        }
    }
    else{
        if (!do_pretrain){
            printf("[Config] [Info] Ignoring pre-training-paths as you did not specify --pretrain.\n");
        }
        else{
            int pre_training_paths_len = cJSON_GetArraySize(pre_training_paths_raw);
            if (pre_training_paths_len == 0){
                printf("[Config] [Fatal] pre-training-paths is empty.\n");
                return 1;
            }
            pre_training_paths = malloc(pre_training_paths_len * sizeof(char*));
            if (!pre_training_paths){
                printf("Failed to allocate memory to parse config.\n");
                return 1;
            }
            for (int index = 0; index < pre_training_paths_len; index++){
                cJSON* item = cJSON_GetArrayItem(pre_training_paths_raw, index);
                if (!cJSON_IsString(item)){
                    printf("[Config] [Fatal] Item %d/%d of pre-training-paths is not a string.\n", index + 1, pre_training_paths_len);
                    return 1;
                }
                pre_training_paths[index] = malloc(strlen(item->valuestring) + 1);
                if (!pre_training_paths[index]){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }
                strcpy(pre_training_paths[index], item->valuestring);
                if (strlen(pre_training_paths[index]) == 0){
                    printf("[Config] [Fatal] Item %d/%d of pre-training-paths isn't a valid file path.\n", index + 1, pre_training_paths_len);
                    return 1;
                }
                if (!file_exists(pre_training_paths[index])){
                    printf("[Config] [Fatal] Item %d/%d of pre-training-paths isn't a valid file path.\n", index + 1, pre_training_paths_len);
                    return 1;
                }
            }
        }
    }

    cJSON* training_dataset_path_raw = cJSON_GetObjectItem(config, "training-dataset-path");
    char* training_dataset_path = NULL;
    if (!cJSON_IsString(training_dataset_path_raw)){
        if (!do_train){
            printf("[Config] [Warning] training-dataset-path is missing/corrupted but --train is not specified therefore this can be ignored.\n");
        }
        else{
            printf("[Config] [Fatal] training-dataset-path is missing/corrupted.\n");
            return 1;
        }
    }
    else{
        if (!do_train){
            printf("[Config] [Info] Ignoring training-dataset-path as you did not specify --train.\n");
        }
        else{
            if (strlen(training_dataset_path_raw->valuestring) == 0){
                printf("[Config] [Fatal] training-dataset-path is not a valid file path.\n");
                return 1;
            }
            if (!file_exists(training_dataset_path_raw->valuestring)){
                printf("[Config] [Fatal] training-dataset-path is not a valid file path.\n");
                return 1;
            }
            else{
                training_dataset_path = malloc(strlen(training_dataset_path_raw->valuestring) + 1);
                if (!training_dataset_path){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }
                strcpy(training_dataset_path, training_dataset_path_raw->valuestring);
            }
        }
    }

    cJSON* pre_train_epochs_raw = cJSON_GetObjectItem(config, "pre-train-epochs");
    int pre_train_epochs = -1;
    if (!cJSON_IsNumber(pre_train_epochs_raw)){
        if (do_pretrain){
            printf("[Config] [Fatal] pre-train-epochs is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] pre-train-epochs is missing/corrupted but you did not specify --pretrain therefore this can be ignored.\n");
        }
    }
    else{
        if (!(isInt(pre_train_epochs_raw->valuedouble))){
            if (do_pretrain){
                printf("[Config] [Fatal] pre-train-epochs is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] pre-train-epochs is supposed to be an int but it is a float, as you did not specify --pretrain this can be ignored.\n");
            }
        }
        else{
            if ((int)(pre_train_epochs_raw->valuedouble) < 1){
                if (do_pretrain){
                    printf("[Config] [Fatal] pre-train-epochs is supposed to be >= 1, but it is set to %d.\n", (int)(pre_train_epochs_raw->valuedouble));
                    return -1;
                }
                else{
                    printf("[Config] [Warning] pre-train-epochs is supposed to be >= 1, but it is set to %d. However as you did not specify --pretrain this can be ignored.\n", (int)(pre_train_epochs_raw->valuedouble));
                }
            }
            else{
                if (do_pretrain){
                    pre_train_epochs = (int)(pre_train_epochs_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring pre-train-epochs as you did not specify --pretrain.\n");
                }
            }
        }
    }

    cJSON* train_epochs_raw = cJSON_GetObjectItem(config, "train-epochs");
    int train_epochs = -1;
    if (!cJSON_IsNumber(train_epochs_raw)){
        if (do_train){
            printf("[Config] [Fatal] train-epochs is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] train-epochs is missing/corrupted but you did not specify --train therefore this can be ignored.\n");
        }
    }
    else{
        if (!(isInt(train_epochs_raw->valuedouble))){
            if (do_train){
                printf("[Config] [Fatal] train-epochs is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] train-epochs is supposed to be an int but it is a float, as you did not specify --train this can be ignored.\n");
            }
        }
        else{
            if ((int)(train_epochs_raw->valuedouble) < 1){
                if (do_train){
                    printf("[Config] [Fatal] train-epochs is supposed to be >= 1, but it is set to %d.\n", (int)(train_epochs_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] train-epochs is supposed to be >= 1, but it is set to %d. However as you did not specify --train this can be ignored.\n", (int)(train_epochs_raw->valuedouble));
                }
            }
            else{
                if (do_train){
                    train_epochs = (int)(train_epochs_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring train-epochs as you did not specify --train.\n");
                }
            }
        }
    }

    cJSON* pre_train_optimizer_raw = cJSON_GetObjectItem(config, "pre-train-optimizer");
    char* pre_train_optimizer = NULL;

    if (!cJSON_IsString(pre_train_optimizer_raw)){
        if (do_pretrain){
            printf("[Config] [Fatal] pre-train-optimizer is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] pre-train-optimizer is missing/corrupted but you did not specify --pretrain therefore this can be ignored.\n");
        }
    }
    else{
        if ((!(strcmp("adam", pre_train_optimizer_raw->valuestring) == 0)) && (!(strcmp("sgd_momentum", pre_train_optimizer_raw->valuestring))) && (!(strcmp("sgd", pre_train_optimizer_raw->valuestring)))){
            if (do_pretrain){
                printf("[Config] [Fatal] pre-train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s.\n", pre_train_optimizer_raw->valuestring);
            }
            else{
                printf("[Config] [Warning] pre-train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s. However as you did not specify --pretrain this can be ignored.\n", pre_train_optimizer_raw->valuestring);
            }
        }
        else{
            if (do_pretrain){
                pre_train_optimizer = malloc(strlen(pre_train_optimizer_raw->valuestring) + 1);
                if (!pre_train_optimizer){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }
                else{
                    strcpy(pre_train_optimizer, pre_train_optimizer_raw->valuestring);
                }
            }
            else{
                printf("[Config] [Info] Ignoring pre-train-optimizer as you did not specify --pretrain.\n");
            }
        }
    }
    
    cJSON* train_optimizer_raw = cJSON_GetObjectItem(config, "train-optimizer");
    char* train_optimizer = NULL;

    if (!cJSON_IsString(train_optimizer_raw)){
        if (do_train){
            printf("[Config] [Fatal] train-optimizer is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] train-optimizer is missing/corrupted but you did not specify --train therefore this can be ignored.\n");
        }
    }
    else{
        if ((!(strcmp("adam", train_optimizer_raw->valuestring) == 0)) && (!(strcmp("sgd_momentum", train_optimizer_raw->valuestring))) && (!(strcmp("sgd", train_optimizer_raw->valuestring)))){
            if (do_train){
                printf("[Config] [Fatal] train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s.\n", train_optimizer_raw->valuestring);
            }
            else{
                printf("[Config] [Warning] train-optimizer is supposed to be either \"adam\", \"sgd_momentum\" or \"sgd\" but it is set to %s. However as you did not specify --train this can be ignored.\n", train_optimizer_raw->valuestring);
            }
        }
        else{
            if (do_train){
                train_optimizer = malloc(strlen(train_optimizer_raw->valuestring) + 1);
                if (!train_optimizer){
                    printf("Failed to allocate memory to parse config.\n");
                    return 1;
                }
                else{
                    strcpy(train_optimizer, train_optimizer_raw->valuestring);
                }
            }
            else{
                printf("[Config] [Info] Ignoring train_optimizer as you did not specify --train.\n");
            }
        }
    }

    cJSON* contextSize_raw = cJSON_GetObjectItem(config, "contextSize");
    int contextSize = -1;

    if (!cJSON_IsNumber(contextSize_raw)){
        printf("[Config] [Fatal] contextSize is missing/corrupted.\n");
        return 1;
    }
    else{
        if (!isInt(contextSize_raw->valuedouble)){
            printf("[Config] [Fatal] contextSize is supposed to be an int but it is a float.\n");
            return 1;
        }
        else{
            if ((int)(contextSize_raw->valuedouble) < 1){
                printf("[Config] [Fatal] contextSize is supposed to be >= 1 but it is set to %d.\n", (int)(contextSize_raw->valuedouble));
                return 1;
            }
            else{
                contextSize = (int)(contextSize_raw->valuedouble);
            }
        }
    }

    cJSON* learningRate_raw = cJSON_GetObjectItem(config, "learningRate");
    double learningRate = 0xdeadbeef;
    if (!cJSON_IsNumber(learningRate_raw)){
        if (do_train || do_pretrain){
            printf("[Config] [Fatal] learningRate is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] learningRate is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (do_train || do_pretrain){
            learningRate = learningRate_raw->valuedouble;
        }
        else{
            printf("[Config] [Info] Ignoring learningRate as you did not specify either --pretrain or --train.\n");
        }
        if (learningRate < 0){
            printf("[Console] [Info] Showing tip anyways :)\n");
            printf("[Config] [Tip] A learningRate < 0 will make the model unlearn the data. This is not recomended, you are on your own, good luck.\n");
        }
    }

    cJSON* maxOutputSize_raw = cJSON_GetObjectItem(config, "maxOutputSize");
    int maxOutputSize = -1;

    if (!cJSON_IsNumber(maxOutputSize_raw)){
        printf("[Config] [Fatal] maxOutputSize is missing/corrupted.\n");
        return 1;
    }
    else{
        if (!isInt(maxOutputSize_raw->valuedouble)){
            printf("[Config] [Fatal] maxOutputSize is supposed to be an int but it is a float.\n");
            return 1;
        }
        else{
            if ((int)(maxOutputSize_raw->valuedouble) < 1){
                printf("[Config] [Fatal] maxOutputSize is supposed to be >= 1 but it is set to %d.\n", (int)(maxOutputSize_raw->valuedouble));
                return 1;
            }
            else{
                maxOutputSize = (int)(maxOutputSize_raw->valuedouble);
            }
        }
    }

    cJSON* batchSize_raw = cJSON_GetObjectItem(config, "batchSize");
    int batchSize = -1;

    if (!cJSON_IsNumber(batchSize_raw)){
        if (do_pretrain || do_train){
            printf("[Config] [Fatal] batchSize is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] batchSize is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(batchSize_raw->valuedouble)){
            if (do_pretrain || do_train){
                printf("[Config] [Fatal] batchSize is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] batchSize is supposed to be an int but it is a float but you did not specify either --pretrain or --train therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(batchSize_raw->valuedouble) < 1){
                if (do_pretrain || do_train){
                    printf("[Config] [Fatal] batchSize is supposed to be >= 1 but it is set to %d.\n", (int)(maxOutputSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] batchSize is supposed to be >= 1 but it is set to %d but you did not specify either --pretrain or --train therefore this can be ignored.\n", (int)(maxOutputSize_raw->valuedouble));
                }
            }
            else{
                if (do_pretrain || do_train){
                    batchSize = (int)(batchSize_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring batchSize as you did not specify either --pretrain or --train.\n");
                }
            }
        }
    }

    cJSON* antiOverfittingOptimisations_raw = cJSON_GetObjectItem(config, "antiOverfittingOptimisations");
    bool antiOverfittingOptimisations = false; //yea i'll be fair here i can't put a cool dummy value.
    
    if (!cJSON_IsBool(antiOverfittingOptimisations_raw)){
        if (do_pretrain || do_train){
            printf("[Config] [Fatal] antiOverfittingOptimisations is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] antiOverfittingOptimisations is missing/corrupted but you did not specify either --pretrain or --train therefore this can be ignored.\n");
        }
    }
    else{
        if (do_pretrain || do_train){
            antiOverfittingOptimisations = cJSON_IsTrue(antiOverfittingOptimisations_raw);
        }
        else{
            printf("[Config] [Info] Ignoring antiOverfittingOptimisations as you did not specify either --pretrain or --train.\n");
        }
    }

    cJSON* embeddingSize_raw = cJSON_GetObjectItem(config, "embeddingSize");
    int embeddingSize = -1;
    if (!cJSON_IsNumber(embeddingSize_raw)){
        if (new){
            printf("[Config] [Fatal] embeddingSize is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] embeddingSize is missing/corrupted but you are loading a model, not creating one therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(embeddingSize_raw->valuedouble)){
            if (new){
                printf("[Config] [Fatal] embeddingSize is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] embeddingSize is supposed to be an int but it is a float, but you are loading a model, not creating a new one therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(embeddingSize_raw->valuedouble) < 1){
                if (new){
                    printf("[Config] [Fatal] embeddingSize is supposed to be >= 1 but it is set to %d.\n", (int)(embeddingSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] embeddingSize is supposed to be >= 1 but it is set to %d, but you are loading a model, not creating a new one therefore this can be ignored.\n");
                }
            }
            else{
                if (new){
                    embeddingSize = (int)(embeddingSize_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring embeddingSize as you are loading a model, not creating a new one.\n");
                }
            }
        }
    }

    cJSON* layersAmount_raw = cJSON_GetObjectItem(config, "layersAmount");
    int layersAmount = -1;
    if (!cJSON_IsNumber(layersAmount_raw)){
        if (new){
            printf("[Config] [Fatal] layersAmount is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] layersAmount is missing/corrupted but you are loading a model, not creating one therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(layersAmount_raw->valuedouble)){
            if (new){
                printf("[Config] [Fatal] layersAmount is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] layersAmount is supposed to be an int but it is a float, but you are loading a model, not creating a new one therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(layersAmount_raw->valuedouble) < 1){
                if (new){
                    printf("[Config] [Fatal] layersAmount is supposed to be >= 1 but it is set to %d.\n", (int)(embeddingSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] layersAmount is supposed to be >= 1 but it is set to %d, but you are loading a model, not creating a new one therefore this can be ignored.\n");
                }
            }
            else{
                if (new){
                    layersAmount = (int)(layersAmount_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring layersAmount as you are loading a model, not creating a new one.\n");
                }
            }
        }
    }

    cJSON* heads_raw = cJSON_GetObjectItem(config, "heads");
    int heads = -1;
    if (!cJSON_IsNumber(heads_raw)){
        if (new){
            printf("[Config] [Fatal] heads is missing/corrutped.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] heads is missing/corrupted but you are loading a model, not creating one therefore this can be ignored.\n");
        }
    }
    else{
        if (!isInt(heads_raw->valuedouble)){
            if (new){
                printf("[Config] [Fatal] heads is supposed to be an int but it is a float.\n");
                return 1;
            }
            else{
                printf("[Config] [Warning] heads is supposed to be an int but it is a float, but you are loading a model, not creating a new one therefore this can be ignored.\n");
            }
        }
        else{
            if ((int)(heads_raw->valuedouble) < 1){
                if (new){
                    printf("[Config] [Fatal] heads is supposed to be >= 1 but it is set to %d.\n", (int)(embeddingSize_raw->valuedouble));
                    return 1;
                }
                else{
                    printf("[Config] [Warning] heads is supposed to be >= 1 but it is set to %d, but you are loading a model, not creating a new one therefore this can be ignored.\n");
                }
            }
            else{
                if (new){
                    heads = (int)(heads_raw->valuedouble);
                }
                else{
                    printf("[Config] [Info] Ignoring heads as you are loading a model, not creating a new one.\n");
                }
            }
        }
    }

    cJSON* biasesinitrange_raw = cJSON_GetObjectItem(config, "biasesinitrange");
    float* biasesinitrange = NULL;

    if (!cJSON_IsArray(biasesinitrange_raw)){
        if (new){
            printf("[Config] [Fatal] biasesinitrange is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] biasesinitrange is missing/corrupted but you are loading a model, not creating a new one therefore this can be ignored.\n");
        }
    }
    else{
        int arr_size = cJSON_GetArraySize(biasesinitrange_raw);
        if (!(arr_size == 2)){
            if (new){
                printf("[Config] [Fatal] biasesinitrange is supposed to contain two elements, it currently contains %d elements.\n", arr_size);
                return 1;
            }
            else{
                printf("[Config] [Warning] biasesinitrange is supposed to contain two elements, it currently contains %d elements but you are loading a model, not creating a new one therefore this can be ignored.\n", arr_size);
            }
        }
        else{
            cJSON* elem_a = cJSON_GetArrayItem(biasesinitrange_raw, 0);
            cJSON* elem_b = cJSON_GetArrayItem(biasesinitrange_raw, 1);
            
            if (!cJSON_IsNumber(elem_a)){
                if (new){
                    printf("[Config] [Fatal] biasesinitrange's elements are supposed to be numbers, however first element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] biasesinitrange's elements are supposed to be numbers, however the first element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }
            
            if (!cJSON_IsNumber(elem_b)){
                if (new){
                    printf("[Config] [Fatal] biasesinitrange's elements are supposed to be numbers, however second element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] biasesinitrange's elements are supposed to be numbers, however the second element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }

            if (new){
                biasesinitrange = malloc(2 * sizeof(double));
                if (!biasesinitrange){
                    printf("Failed memory allocation to parse config.\n");
                    return 1;
                }
                biasesinitrange[0] = (float)(elem_a->valuedouble);
                biasesinitrange[1] = (float)(elem_b->valuedouble);
            }
            else{
                printf("[Config] [Info] Ignoring biasesinitrange as you are loading a model, not creating a new one.\n");
            }
        }
    }

    cJSON* embeddinginitrange_raw = cJSON_GetObjectItem(config, "embeddinginitrange");
    float* embeddinginitrange = NULL;

    if (!cJSON_IsArray(embeddinginitrange_raw)){
        if (new){
            printf("[Config] [Fatal] embeddinginitrange is missing/corrupted.\n");
            return 1;
        }
        else{
            printf("[Config] [Warning] embeddinginitrange is missing/corrupted but you are loading a model, not creating a new one therefore this can be ignored.\n");
        }
    }
    else{
        int arr_size = cJSON_GetArraySize(embeddinginitrange_raw);
        if (!(arr_size == 2)){
            if (new){
                printf("[Config] [Fatal] embeddinginitrange is supposed to contain two elements, it currently contains %d elements.\n", arr_size);
                return 1;
            }
            else{
                printf("[Config] [Warning] embeddinginitrange is supposed to contain two elements, it currently contains %d elements but you are loading a model, not creating a new one therefore this can be ignored.\n", arr_size);
            }
        }
        else{
            cJSON* elem_a = cJSON_GetArrayItem(embeddinginitrange_raw, 0);
            cJSON* elem_b = cJSON_GetArrayItem(embeddinginitrange_raw, 1);
            
            if (!cJSON_IsNumber(elem_a)){
                if (new){
                    printf("[Config] [Fatal] embeddinginitrange's elements are supposed to be numbers, however first element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] embeddinginitrange's elements are supposed to be numbers, however the first element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }
            
            if (!cJSON_IsNumber(elem_b)){
                if (new){
                    printf("[Config] [Fatal] embeddinginitrange's elements are supposed to be numbers, however second element is an impostor.\n");
                    return 1;
                }
                else{
                    printf("[Config] [Warning] embeddinginitrange's elements are supposed to be numbers, however the second element is an impostor, but because you are loading a model and not creating a new one this can be ignored.\n");
                }
            }

            if (new){
                embeddinginitrange = malloc(2 * sizeof(double));
                if (!embeddinginitrange){
                    printf("Failed memory allocation to parse config.\n");
                    return 1;
                }
                embeddinginitrange[0] = (float)(elem_a->valuedouble);
                embeddinginitrange[1] = (float)(elem_b->valuedouble);
            }
            else{
                printf("[Config] [Info] Ignoring embeddinginitrange as you are loading a model, not creating a new one.\n");
            }
        }
    }

    float* he_init(float fan_in){
        float* returns = malloc(2 * sizeof(float));
        if (!returns){
            printf("Failed memory allocation for weights initalisation range calculation.\n");
            return NULL;
        }
        float range = sqrtf(2.0f / fan_in);
        returns[0] = -range;
        returns[1] = range;
        return returns;
    }

    float* weightsinitrange = NULL;
    if (do_pretrain || do_train){
        printf("Calculating weight initalisation range with he init...\n");
        weightsinitrange = he_init(embeddingSize);
        if (!weightsinitrange){
            return 1;
        }
        printf("Calculated weight initalisation range with he init.\n");
    }

    printf("Reading vocabulary file (vocabulary.json)...\n");
    char* vocab_file = read_file("vocabulary.json");
    if (!vocab_file){
        printf("Failed to read vocabulary file.\n");
        return 1;
    }
    printf("Read vocabulary file.\n");
    printf("Parsing vocabulary...\n");
    cJSON* vocab = cJSON_Parse(vocab_file);
    if (!vocab){
        printf("Failed to parse vocabulary.\n");
        return 1;
    }
    if (!cJSON_IsArray(vocab)){
        printf("Vocabulary is corrutped.\n");
        return 1;
    }
    printf("Parsed vocabulary.\n");
    long long timer_ = timer();
    printf("Computing id to token table...\n");
    
    int vocab_len = cJSON_GetArraySize(vocab);
    int vocab_toksize = 0;
    int* vocab_per_toksize = NULL;
    bool* where_gap = NULL;
    int vocab_per_toksize_len = 0;
    int where_gap_len = 0;
    int gap_size = 0;
    int lastId = -1;  // Start from -1 to allow 0 as first ID
    int cursor = 0;
    int padlen = strlen("PAD_NO_TOK_HERE") + 1;
    char* padtok = malloc(padlen);
    if (!padtok){
        printf("Failed to allocate memory to compute id to token table...\n");
        return 1;
    }
    strcpy(padtok, "PAD_NO_TOK_HERE");

    cJSON* item = vocab->child;
    int index = 0;

    while (item != NULL){
        if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) != 2) {
            printf("Vocabulary item %d/%d is corrupted.\n", index + 1, vocab_len);
            return 1;
        }

        cJSON* item_elem_a = item->child; // string
        cJSON* item_elem_b = item_elem_a->next; // int ID

        if (!cJSON_IsString(item_elem_a) || !cJSON_IsNumber(item_elem_b)) {
            printf("Vocabulary item %d/%d is corrupted.\n", index + 1, vocab_len);
            return 1;
        }

        int item_strlen = strlen(item_elem_a->valuestring) + 1; // +1 for null terminator

        // Grow vocab_per_toksize array
        vocab_per_toksize = realloc(vocab_per_toksize, (vocab_per_toksize_len + 1) * sizeof(int));
        if (!vocab_per_toksize) {
            printf("Failed to allocate memory for vocab_per_toksize.\n");
            return 1;
        }
        vocab_per_toksize[vocab_per_toksize_len++] = item_strlen;

        vocab_toksize += item_strlen;

        int current_id = (int)(item_elem_b->valuedouble);

        // Fill any gaps between lastId and current_id
        while (lastId + 1 < current_id) {
            where_gap = realloc(where_gap, (where_gap_len + 1) * sizeof(bool));
            if (!where_gap) {
                printf("Failed to allocate memory for where_gap.\n");
                return 1;
            }
            where_gap[where_gap_len++] = true;  // there's a gap here
            lastId++;
            gap_size++;
            cursor++;
        }

        // Mark current ID as non-gap
        where_gap = realloc(where_gap, (where_gap_len + 1) * sizeof(bool));
        if (!where_gap) {
            printf("Failed to allocate memory for where_gap.\n");
            return 1;
        }
        where_gap[where_gap_len++] = false;

        lastId = current_id;
        cursor++;
        item = item->next;
        index++;
    }
    char** id_to_tok = malloc((gap_size + vocab_len) * sizeof(char*));
    if (!id_to_tok){
        printf("Failed memory allocation to compute id to token table.\n");
        return 1;
    }
    int vocab_index = 0;
    cJSON* item_outer = vocab->child;
    char* item_ = NULL;
    for (int index = 0; index < gap_size + vocab_len; index++){
        if (where_gap[index] == false){
            item_ = item_outer->child->valuestring;
            id_to_tok[index] = malloc(vocab_per_toksize[vocab_index]);
            if (!id_to_tok[index]){
                printf("Failed memory allocation to compute id to token table.\n");
                return 1;
            }
            strcpy(id_to_tok[index], item_);
            item_outer = item_outer->next;
            vocab_index++;
        }
        else{
            id_to_tok[index] = padtok;
        }
    }
    printf("Computed id to token table in %lldms.\n", timer_end(timer_));
    printf("Computing token to id data...\n");
    timer_ = timer();

    typedef struct { //I have joined the dark side of structs.
        char* token;
        int id;
    } TokenEntry;

    TokenEntry* token_to_id_tokensort = malloc((gap_size + vocab_len) * sizeof(TokenEntry));

    for (int index = 0; index < gap_size + vocab_len; index++){
        token_to_id_tokensort[index].token = id_to_tok[index];
        token_to_id_tokensort[index].id = index;
    }
    
    int cmp_tokens(const void* a, const void* b) {
        return strcmp(((TokenEntry*)a)->token, ((TokenEntry*)b)->token);
    }

    qsort(token_to_id_tokensort, vocab_len + gap_size, sizeof(TokenEntry), cmp_tokens);
    
    free(vocab_per_toksize);
    free(where_gap);
    printf("Computed token to id data in %lldms.\n", timer_end(timer_));

    int token_to_id(char* tok) { //binary search bruh
        int left = 0;
        int right = (vocab_len + gap_size) - 1;

        while (left <= right) {
            int mid = (left + right) / 2;
            int cmp = strcmp(tok, token_to_id_tokensort[mid].token);

            if (cmp == 0) {
                return token_to_id_tokensort[mid].id;
            }

            if (cmp < 0) {
                right = mid - 1;
            } 
            else {
                left = mid + 1;
            }
        }

        return -1;
    }

    char* id_to_token(int id){
        if (id > gap_size + vocab_len){
            return NULL;
        }
        else{
            if (id < 0){
                return NULL;
            }
            else{
                char* res = id_to_tok[id];
                if (strcmp(res, "PAD_NO_TOK_HERE") == 0){
                    return NULL;
                }
                else{
                    return res;
                }
            }
        }
    }

    int* tokenize(char* str_) {
        int len = strlen(str_);
        if (len == 0) return NULL;

        char* str = malloc(len + 1);
        if (!str) {
            printf("Failed to allocate memory to tokenize text.\n");
            return NULL;
        }
        strcpy(str, str_);

        int* tokenized = NULL;
        int tokenized_len = 0;
        int consumed = 0;

        while (consumed < len) {
            int cursor = len - consumed;
            int found = 0;

            while (cursor > 0) {
                char saved = str[consumed + cursor];
                str[consumed + cursor] = '\0';

                int tok_id = token_to_id(str + consumed);

                str[consumed + cursor] = saved;

                if (tok_id != -1) {
                    int* new_arr = realloc(tokenized, (tokenized_len + 2) * sizeof(int));
                    if (!new_arr) {
                        printf("Failed to allocate memory to tokenize text.\n");
                        free(tokenized);
                        free(str);
                        return NULL;
                    }
                    tokenized = new_arr;
                    tokenized_len++;
                    tokenized[tokenized_len] = tok_id;
                    consumed += cursor;
                    found = 1;
                    break;
                }
                cursor--;
            }

            if (!found) {
                free(tokenized);
                free(str);
                return NULL;
            }
        }

        free(str);

        if (tokenized) {
            tokenized[0] = tokenized_len;
        }

        return tokenized;
    }

    float temperature = 0.7;
    int step_num = 0;
    typedef struct{
        float beta1;
        float beta2;
        float epsilon;
        int t;
    } ap;
    ap adam_params;
    adam_params.beta1 = 0.9;
    adam_params.beta2 = 0.98;
    adam_params.epsilon = 1e-9;
    adam_params.t = 0;

    printf("Initalizing model...\n");
    long long timer___ = timer();
    printf("Initalizing layers...\n");
    timer_ = timer();

    float random_range(float* ran){
        float min = ran[0];
        float max = ran[1];
        return min + ((float)rand() / (float)RAND_MAX) * (max - min);
    }

    typedef struct {
        struct {
            float* normalize_1;
            struct {
                struct {
                    float* query;
                    float* key;
                    float* value;
                } *heads;
                float* output;
            } attention;
            float* normalize_2;
            struct {
                float* grow;
                float* shrink;
            } feed_forward;
        } weights;
        struct {
            float* normalize_1;
            struct {
                struct {
                    float* query;
                    float* key;
                    float* value;
                } *heads;
                float* output;
            } attention;
            float* normalize_2;
            struct {
                float* grow;
                float* shrink;
            } feed_forward;
        } biases;
    } layer;

    typedef struct {
        float* weights;
        float* biases;
    } vp;

    char* mname(const char* fmt, ...) {
        if (!fmt) return NULL;

        char pidbuf[32];
        snprintf(pidbuf, sizeof pidbuf, "%lu", getPid());

        va_list ap;
        va_start(ap, fmt);

        // compute formatted length (excl. '\0')
        int needed;
#if defined(_WIN32)
        va_list ap1; va_copy(ap1, ap);
        needed = _vscprintf(fmt, ap1);
        va_end(ap1);
#else
        va_list ap1; va_copy(ap1, ap);
        needed = vsnprintf(NULL, 0, fmt, ap1);
        va_end(ap1);
#endif
        if (needed < 0) { va_end(ap); return NULL; }

        size_t prefix_len = strlen(pidbuf) + 2; // '/' and '_'
        size_t total = prefix_len + (size_t)needed + 1;

        char* out = malloc(total);
        if (!out) {
            printf("Failed to allocate memory to generate shared memory name.\n");
            va_end(ap);
            return NULL;
        }

        int wrote = snprintf(out, total, "/%s_", pidbuf);
        (void)vsnprintf(out + wrote, total - (size_t)wrote, fmt, ap);
        va_end(ap);
        return out;
    }

    //Chat we cookin
    if (new){
        layer* layers = malloc(layersAmount * sizeof(layer));
        if (!layers){
            printf("Failed to allocate memory to initalize layers.\n");
            return 1;
        }
        for (int index = 0; index < layersAmount; index++){
            printf("Initalizing layer %d/%d...\n", index + 1, layersAmount);
            long long timer__ = timer();
            char* name = mname("layers[%d].weights.normalize_1", index);
            if (!name){
                return 1;
            }
            layers[index].weights.normalize_1 = smalloc(embeddingSize * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].weights.normalize_2", index);
            if (!name){
                return 1;
            }
            layers[index].weights.normalize_2 = smalloc(embeddingSize * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].biases.normalize_1", index);
            if (!name){
                return 1;
            }
            layers[index].biases.normalize_1 = smalloc(embeddingSize * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].biases.normalize_2", index);
            if (!name){
                return 1;
            }
            layers[index].biases.normalize_2 = smalloc(embeddingSize * 3 * sizeof(float), name);
            layers[index].weights.attention.heads = malloc(heads * sizeof(*layers[index].weights.attention.heads));
            layers[index].biases.attention.heads = malloc(heads * sizeof(*layers[index].biases.attention.heads));
            free(name);
            name = mname("layers[%d].weights.attention.output", index);
            if (!name){
                return 1;
            }
            layers[index].weights.attention.output = smalloc(embeddingSize * (embeddingSize * heads) * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].biases.attention.output", index);
            if (!name){
                return 1;
            }
            layers[index].biases.attention.output = smalloc(embeddingSize * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].weights.feed_forward.grow", index);
            if (!name){
                return 1;
            }
            layers[index].weights.feed_forward.grow = smalloc(embeddingSize * (embeddingSize * 4) * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].weights.feed_forward.shrink", index);
            if (!name){
                return 1;
            }
            layers[index].weights.feed_forward.shrink = smalloc(embeddingSize * (embeddingSize * 4) * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].biases.feed_forward.grow", index);
            if (!name){
                return 1;
            }
            layers[index].biases.feed_forward.grow = smalloc((embeddingSize * 4) * 3 * sizeof(float), name);
            free(name);
            name = mname("layers[%d].biases.feed_forward.shrink", index);
            if (!name){
                return 1;
            }
            layers[index].biases.feed_forward.shrink = smalloc(embeddingSize * 3 * sizeof(float), name);
            free(name);

            if (!layers[index].weights.normalize_1){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].weights.normalize_2){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].biases.normalize_1){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].biases.normalize_2){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].weights.attention.heads){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].biases.attention.heads){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].weights.attention.output){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].biases.attention.output){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].weights.feed_forward.grow){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].weights.feed_forward.shrink){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].biases.feed_forward.grow){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }
            if (!layers[index].biases.feed_forward.shrink){
                printf("Failed to allocate memory to initalize layers.\n");
                return 1;
            }

            for (int subindex = 0; subindex < embeddingSize; subindex++){
                layers[index].weights.normalize_1[subindex * 3] = random_range(weightsinitrange);
                layers[index].weights.normalize_2[subindex * 3] = random_range(weightsinitrange);
                layers[index].biases.normalize_1[subindex * 3] = random_range(biasesinitrange);
                layers[index].biases.normalize_2[subindex * 3] = random_range(biasesinitrange);
            }

            for (int subindex = 0; subindex < heads; subindex++){
                name = mname("layers[%d].weights.attention.heads[%d].query", index, subindex);
                if (!name){
                    return 1;
                }
                layers[index].weights.attention.heads[subindex].query = smalloc(embeddingSize * embeddingSize * 3 * sizeof(float), name);
                free(name);
                name = mname("layers[%d].weights.attention.heads[%d].key", index, subindex);
                if (!name){
                    return 1;
                }
                layers[index].weights.attention.heads[subindex].key = smalloc(embeddingSize * embeddingSize * 3 * sizeof(float), name);
                free(name);
                name = mname("layers[%d].weights.attention.heads[%d].value", index, subindex);
                if (!name){
                    return 1;
                }
                layers[index].weights.attention.heads[subindex].value = smalloc(embeddingSize * embeddingSize * 3 * sizeof(float), name);
                
                free(name);
                name = mname("layers[%d].biases.attention.heads[%d].query", index, subindex);
                if (!name){
                    return 1;
                }
                layers[index].biases.attention.heads[subindex].query = smalloc(embeddingSize * 3 * sizeof(float), name);
                free(name);
                name = mname("layers[%d].biases.attention.heads[%d].key", index, subindex);
                if (!name){
                    return 1;
                }
                layers[index].biases.attention.heads[subindex].key = smalloc(embeddingSize * 3 * sizeof(float), name);
                free(name);
                name = mname("layers[%d].biases.attention.heads[%d].value", index, subindex);
                if (!name){
                    return 1;
                }
                layers[index].biases.attention.heads[subindex].value = smalloc(embeddingSize * 3 * sizeof(float), name);
                free(name);

                if (!layers[index].weights.attention.heads[subindex].query){
                    printf("Failed to allocate memory to initalize layers.\n");
                    return 1;
                }
                if (!layers[index].weights.attention.heads[subindex].key){
                    printf("Failed to allocate memory to initalize layers.\n");
                    return 1;
                }
                if (!layers[index].weights.attention.heads[subindex].value){
                    printf("Failed to allocate memory to initalize layers.\n");
                    return 1;
                }

                if (!layers[index].biases.attention.heads[subindex].query){
                    printf("Failed to allocate memory to initalize layers.\n");
                    return 1;
                }
                if (!layers[index].biases.attention.heads[subindex].key){
                    printf("Failed to allocate memory to initalize layers.\n");
                    return 1;
                }
                if (!layers[index].biases.attention.heads[subindex].value){
                    printf("Failed to allocate memory to initalize layers.\n");
                    return 1;
                }

                for (int subindex_ = 0; subindex_ < embeddingSize * embeddingSize; subindex_++){
                    layers[index].weights.attention.heads[subindex].query[subindex_ * 3] = random_range(weightsinitrange);
                    layers[index].weights.attention.heads[subindex].key[subindex_ * 3] = random_range(weightsinitrange);
                    layers[index].weights.attention.heads[subindex].value[subindex_ * 3] = random_range(weightsinitrange);
                }

                for (int subindex_ = 0; subindex_ < embeddingSize; subindex_++){
                    layers[index].biases.attention.heads[subindex].query[subindex_ * 3] = random_range(biasesinitrange);
                    layers[index].biases.attention.heads[subindex].key[subindex_ * 3] = random_range(biasesinitrange);
                    layers[index].biases.attention.heads[subindex].value[subindex_ * 3] = random_range(biasesinitrange);
                }
            }

            for (int subindex = 0; subindex < embeddingSize * (embeddingSize * heads); subindex++){
                layers[index].weights.attention.output[subindex * 3] = random_range(weightsinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize; subindex++){
                layers[index].biases.attention.output[subindex * 3] = random_range(biasesinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize * (embeddingSize * 4); subindex++){
                layers[index].weights.feed_forward.grow[subindex * 3] = random_range(weightsinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize * 4; subindex++){
                layers[index].biases.feed_forward.grow[subindex * 3] = random_range(biasesinitrange);
            }

            for (int subindex = 0; subindex < (embeddingSize * 4) * embeddingSize; subindex++){
                layers[index].weights.feed_forward.shrink[subindex * 3] = random_range(weightsinitrange);
            }

            for (int subindex = 0; subindex < embeddingSize; subindex++){
                layers[index].biases.feed_forward.shrink[subindex * 3] = random_range(biasesinitrange);
            }

            printf("Initalized layer %d/%d in %lldms.\n", index + 1, layersAmount, timer_end(timer__));
        }

        printf("Initalized layers in %lldms\n", timer_end(timer_));

        printf("Initalizing embeddings...\n");
        timer_ = timer();
        float** embeddings = malloc((vocab_len + gap_size) * sizeof(float*));
        if (!embeddings){
            printf("Failed to allocate memory to initalize embeddings.\n");
            return 1;
        }

        for (int index = 0; index < vocab_len + gap_size; index++){
            if (strcmp(id_to_tok[index], "PAD_NO_TOK_HERE") == 0){
                embeddings[index] = NULL;
                continue;
            }
            char* name = mname("embeddings[%d]", index);
            if (!name){
                return 1;
            }
            embeddings[index] = smalloc(embeddingSize * 3 * sizeof(float), name);
            free(name);
            if (!embeddings[index]){
                printf("Failed to allocate memory to initalize embeddings.\n");
                return 1;
            }
            for (int subindex = 0; subindex < embeddingSize; subindex++){
                embeddings[index][subindex * 3] = random_range(embeddinginitrange);
            }
        }

        printf("Initalized embeddings in %lldms.\n", timer_end(timer_));
        
        printf("Initalizing vocabulary projection weights and biases.\n");
        timer_ = timer();
        
        vp vocab_projection;
        
        char* name = mname("vocab_projection.weights");
        if (!name){
            return 1;
        }
        vocab_projection.weights = smalloc(vocab_len * embeddingSize * 3 * sizeof(float), name);
        free(name);
        name = mname("vocab_projection.biases");
        if (!name){
            return 1;
        }
        vocab_projection.biases = smalloc(vocab_len * 3 * sizeof(float), name);
        free(name);
        
        if (!vocab_projection.weights){
            printf("Failed memory allocation to initalize vocabulary projection.\n");
            return 1;
        }
        if (!vocab_projection.biases){
            printf("Failed memory allocation to initalize vocabulary projection.\n");
            return 1;
        }

        for (int index = 0; index < vocab_len * embeddingSize; index++){
            vocab_projection.weights[index * 3] = random_range(weightsinitrange);
        }
        for (int index = 0; index < vocab_len; index++){
            vocab_projection.biases[index * 3] = random_range(biasesinitrange);
        }

        printf("Initalized vocabulary projection weights and biases in %lldms.\n", timer_end(timer_));
        printf("Initalized model in %lldms.\n", timer_end(timer___));
    }
    else{
        if (load){
            printf("Opening model file (\"%s\").\n", model_location);
            long long timer_ = timer();
            mz_zip_archive zipfile;
            memset(&zipfile, 0, sizeof(zipfile));
            if (!mz_zip_reader_init_file(&zipfile, model_location, 0)){
                printf("Failed to open model file (\"%s\").\n", model_location);
                return 1;
            }
            printf("Opened model file in %lldms.\n", timer_end(timer_));

            int n_files = (int)(mz_zip_reader_get_num_files(&zipfile));
            printf("Loading model...\n");
            timer_ = timer();
            char*** files = malloc(n_files * sizeof(char**));
            size_t* files_len = malloc(n_files * sizeof(size_t));
            if (!files_len){
                printf("Failed to allocate memory to load model.\n");
                return 1;
            }
            if (!files){
                printf("Failed to allocate memory to load model.\n");
                return 1;
            }
            for (int index = 0; index < n_files; index++){
                files[index] = malloc(2 * sizeof(char*));
                if (!files[index]){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
                mz_zip_archive_file_stat file_info;
                if (mz_zip_reader_file_stat(&zipfile, index, &file_info)) {
                    if ((size_t)(file_info.m_uncomp_size) == 0){
                        printf("Corrupted model file.\n");
                        return 1;
                    }
                    int filename_len = strlen(file_info.m_filename) + 1;
                    files[index][0] = malloc(filename_len);
                    files[index][1] = malloc((size_t)(file_info.m_uncomp_size));
                    files_len[index] = (size_t)(file_info.m_uncomp_size);
                }
                else{
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                if (!files[index][0]){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
                if (!files[index][1]){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
                
                strcpy(files[index][0], file_info.m_filename);
                if (!mz_zip_reader_extract_to_mem(&zipfile, index, files[index][1], (size_t)(file_info.m_uncomp_size), 0)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
            }

            mz_zip_reader_end(&zipfile);

            bool found_model_meta = false;
            int model_meta_index = -1;

            for (int index = 0; index < n_files; index++){
                //search for model_meta
                if (strcmp(files[index][0], "model_meta.json") == 0){
                    found_model_meta = true;
                    model_meta_index = index;
                    
                    char* model_meta = realloc(files[index][1], files_len[index] + 1);
                    if (!model_meta){
                        printf("Failed memory allocation to load model.\n");
                        return 1; //exit, os will reclaim mem
                    }
                    model_meta[files_len[index]] = '\0';
                    files[index][1] = model_meta;
                    files_len[index]++;
                    
                    if (strlen(model_meta) == 0){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    break;
                }
                else{
                    continue;
                }
            }

            if (!found_model_meta){
                printf("Model file is corrupted.\n");
                return 1;
            }

            cJSON* model_meta = cJSON_Parse(files[model_meta_index][1]);
            if (!model_meta){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsObject(model_meta)){
                printf("Model file is corrupted.\n");
                return 1;
            }

            embeddingSize_raw = cJSON_GetObjectItem(model_meta, "embeddingSize");
            if (!cJSON_IsNumber(embeddingSize_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!isInt(embeddingSize_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if ((int)(embeddingSize_raw->valuedouble) < 1){
                printf("Model file is corrupted.\n");
                return 1;
            }
            embeddingSize = (int)(embeddingSize_raw->valuedouble);

            layersAmount_raw = cJSON_GetObjectItem(model_meta, "layersAmount");
            if (!cJSON_IsNumber(layersAmount_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!isInt(layersAmount_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if ((int)(layersAmount_raw->valuedouble) < 1){
                printf("Model file is corrupted.\n");
                return 1;
            }
            layersAmount = (int)(layersAmount_raw->valuedouble);

            heads_raw = cJSON_GetObjectItem(model_meta, "heads");
            if (!cJSON_IsNumber(heads_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!isInt(heads_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if ((int)(heads_raw->valuedouble) < 1){
                printf("Model file is corrupted.\n");
                return 1;
            }
            heads = (int)(heads_raw->valuedouble);

            biasesinitrange_raw = cJSON_GetObjectItem(model_meta, "biasesinitrange");
            if (!cJSON_IsArray(biasesinitrange_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (cJSON_GetArraySize(biasesinitrange_raw) != 2){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(biasesinitrange_raw, 0))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(biasesinitrange_raw, 1))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!biasesinitrange){
                biasesinitrange = malloc(2 * sizeof(float));
                if (!biasesinitrange){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
            }
            biasesinitrange[0] = (float)(cJSON_GetArrayItem(biasesinitrange_raw, 0)->valuedouble);
            biasesinitrange[1] = (float)(cJSON_GetArrayItem(biasesinitrange_raw, 1)->valuedouble);
            
            embeddinginitrange_raw = cJSON_GetObjectItem(model_meta, "embeddinginitrange");
            if (!cJSON_IsArray(embeddinginitrange_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (cJSON_GetArraySize(embeddinginitrange_raw) != 2){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(embeddinginitrange_raw, 0))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetArrayItem(embeddinginitrange_raw, 1))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!embeddinginitrange){
                embeddinginitrange = malloc(2 * sizeof(float));
                if (!embeddinginitrange){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
            }
            embeddinginitrange[0] = (float)(cJSON_GetArrayItem(embeddinginitrange_raw, 0)->valuedouble);
            embeddinginitrange[1] = (float)(cJSON_GetArrayItem(embeddinginitrange_raw, 1)->valuedouble);
            
            cJSON* adam_params_raw = cJSON_GetObjectItem(model_meta, "adam_params");
            if (!cJSON_IsObject(adam_params_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "beta1"))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "beta2"))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "epsilon"))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!cJSON_IsNumber(cJSON_GetObjectItem(adam_params_raw, "t"))){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!isInt(cJSON_GetObjectItem(adam_params_raw, "t")->valuedouble)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if ((int)(cJSON_GetObjectItem(adam_params_raw, "t")->valuedouble) < 0){
                printf("Model file is corrupted.\n");
                return 1;
            }
            adam_params.beta1 = (float)(cJSON_GetObjectItem(adam_params_raw, "beta1")->valuedouble);
            adam_params.beta2 = (float)(cJSON_GetObjectItem(adam_params_raw, "beta2")->valuedouble);
            adam_params.epsilon = (float)(cJSON_GetObjectItem(adam_params_raw, "epsilon")->valuedouble);
            adam_params.t = (int)(cJSON_GetObjectItem(adam_params_raw, "t")->valuedouble);
            
            cJSON* step_num_raw = cJSON_GetObjectItem(model_meta, "step_num");
            if (!cJSON_IsNumber(step_num_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if (!isInt(step_num_raw->valuedouble)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            if ((int)(step_num_raw->valuedouble) < 0){
                printf("Model file is corrupted.\n");
                return 1;
            }
            step_num = (int)(step_num_raw->valuedouble);

            cJSON* transformer_structure = cJSON_GetObjectItem(model_meta, "transformer_structure");
            if (!cJSON_IsObject(transformer_structure)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            cJSON* layers_raw = cJSON_GetObjectItem(transformer_structure, "layers");
            if (!cJSON_IsArray(layers_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }

            if (layersAmount != cJSON_GetArraySize(layers_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            
            float* loadFloats(cJSON* patharr, char* allocname){
                if (!allocname){
                    exit(1);
                }
                if (!cJSON_IsArray(patharr)){
                    printf("Model file is corrupted.\n");
                    exit(1);
                }
                if (cJSON_GetArraySize(patharr) < 1){
                    printf("Model file is corrupted.\n");
                    exit(1);
                }
                size_t total_files_size = 0;
                int total_files = 0;
                int* files_indexes = malloc(cJSON_GetArraySize(patharr) * sizeof(int));
                if (!files_indexes){
                    printf("Failed to allocate memory to load model.\n");
                    exit(1);
                }
                for (int index = 0; index < cJSON_GetArraySize(patharr); index++){
                    cJSON* item = cJSON_GetArrayItem(patharr, index);
                    if (!cJSON_IsString(item)){
                        printf("Model file is corrupted.\n");
                        exit(1);
                    }
                    bool found = false;
                    for (int subindex = 0; subindex < n_files; subindex++){
                        if (!files[subindex][0]){
                            continue;
                        }
                        if (strcmp(item->valuestring, files[subindex][0]) == 0){
                            found = true;
                            total_files_size += files_len[subindex];
                            files_indexes[index] = subindex;
                            total_files++;
                            break;
                        }
                    }
                    if (!found){
                        printf("Model file is corrupted.\n");
                        exit(1);
                    }
                }

                float* floatarr = smalloc(total_files_size, allocname);
                free(allocname);
                if (!floatarr){
                    printf("Failed to allocate memory to load model.\n");
                    exit(1);
                }
                size_t curr_w = 0;
                for (int index = 0; index < total_files; index++){
                    //Chars are only one byte and we wanna count in bytes here therefore let's make c
                    //shut the fuck up.
                    memcpy(((char*)floatarr) + curr_w, files[files_indexes[index]][1], files_len[files_indexes[index]]);
                    free(files[files_indexes[index]][0]);
                    free(files[files_indexes[index]][1]);
                    files[files_indexes[index]][0] = NULL;
                    files[files_indexes[index]][1] = NULL;

                    curr_w += files_len[files_indexes[index]];
                }

                free(files_indexes);

                return floatarr;
            }

            layer* layers = malloc(layersAmount * sizeof(layer));
            if (!layers){
                printf("Failed memory allocation to load model.\n");
                return 1;
            }

            for (int index = 0; index < cJSON_GetArraySize(layers_raw); index++){
                cJSON* layer_curr = cJSON_GetArrayItem(layers_raw, index);
                if (!cJSON_IsObject(layer_curr)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                cJSON* weights_lc = cJSON_GetObjectItem(layer_curr, "weights");
                if (!cJSON_IsObject(weights_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                cJSON* normalize_1_lc = cJSON_GetObjectItem(weights_lc, "normalize_1");
                if (!cJSON_IsArray(normalize_1_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].weights.normalize_1 = loadFloats(normalize_1_lc, mname("layers[%d].weights.normalize_1", index));
                
                cJSON* normalize_2_lc = cJSON_GetObjectItem(weights_lc, "normalize_2");
                if (!cJSON_IsArray(normalize_2_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].weights.normalize_2 = loadFloats(normalize_2_lc, mname("layers[%d].weights.normalize_2", index));

                cJSON* attention_lc = cJSON_GetObjectItem(weights_lc, "attention");
                if (!cJSON_IsObject(attention_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                cJSON* heads_lc = cJSON_GetObjectItem(attention_lc, "heads");
                if (!cJSON_IsArray(heads_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                if (cJSON_GetArraySize(heads_lc) != heads){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                layers[index].weights.attention.heads = malloc(heads * sizeof(*layers[index].weights.attention.heads));
                if (!layers[index].weights.attention.heads){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }

                for (int subindex = 0; subindex < heads; subindex++){
                    if (!cJSON_IsObject(cJSON_GetArrayItem(heads_lc, subindex))){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    cJSON* query_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "query");
                    cJSON* key_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "key");
                    cJSON* value_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "value");
                    if (!cJSON_IsArray(query_lh_lc)){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(key_lh_lc)){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(value_lh_lc)){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    layers[index].weights.attention.heads[subindex].query = loadFloats(query_lh_lc, mname("layers[%d].weights.attention.heads[%d].query", index, subindex));
                    layers[index].weights.attention.heads[subindex].key = loadFloats(key_lh_lc, mname("layers[%d].weights.attention.heads[%d].key", index, subindex));
                    layers[index].weights.attention.heads[subindex].value = loadFloats(value_lh_lc, mname("layers[%d].weights.attention.heads[%d].value", index, subindex));
                }
                
                cJSON* attn_o_lc = cJSON_GetObjectItem(attention_lc, "output");
                if (!cJSON_IsArray(attn_o_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].weights.attention.output = loadFloats(attn_o_lc, mname("layers[%d].weights.attention.output", index));

                cJSON* ffw_lc = cJSON_GetObjectItem(weights_lc, "feed_forward");
                if (!cJSON_IsObject(ffw_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                cJSON* ffw_grow_lc = cJSON_GetObjectItem(ffw_lc, "grow");
                cJSON* ffw_shrink_lc = cJSON_GetObjectItem(ffw_lc, "shrink");
                if (!cJSON_IsArray(ffw_grow_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                if (!cJSON_IsArray(ffw_shrink_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].weights.feed_forward.grow = loadFloats(ffw_grow_lc, mname("layers[%d].weights.feed_forward.grow", index));
                layers[index].weights.feed_forward.shrink = loadFloats(ffw_shrink_lc, mname("layers[%d].weights.feed_forward.shrink", index));


                weights_lc = cJSON_GetObjectItem(layer_curr, "biases");
                if (!cJSON_IsObject(weights_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                normalize_1_lc = cJSON_GetObjectItem(weights_lc, "normalize_1");
                if (!cJSON_IsArray(normalize_1_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].biases.normalize_1 = loadFloats(normalize_1_lc, mname("layers[%d].biases.normalize_1", index));

                normalize_2_lc = cJSON_GetObjectItem(weights_lc, "normalize_2");
                if (!cJSON_IsArray(normalize_2_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].biases.normalize_2 = loadFloats(normalize_2_lc, mname("layers[%d].biases.normalize_2", index));

                attention_lc = cJSON_GetObjectItem(weights_lc, "attention");
                if (!cJSON_IsObject(attention_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                heads_lc = cJSON_GetObjectItem(attention_lc, "heads");
                if (!cJSON_IsArray(heads_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                if (cJSON_GetArraySize(heads_lc) != heads){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                layers[index].biases.attention.heads = malloc(heads * sizeof(*layers[index].biases.attention.heads));
                if (!layers[index].biases.attention.heads){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }

                for (int subindex = 0; subindex < heads; subindex++){
                    if (!cJSON_IsObject(cJSON_GetArrayItem(heads_lc, subindex))){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    cJSON* query_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "query");
                    cJSON* key_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "key");
                    cJSON* value_lh_lc = cJSON_GetObjectItem(cJSON_GetArrayItem(heads_lc, subindex), "value");
                    if (!cJSON_IsArray(query_lh_lc)){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(key_lh_lc)){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    if (!cJSON_IsArray(value_lh_lc)){
                        printf("Model file is corrupted.\n");
                        return 1;
                    }
                    layers[index].biases.attention.heads[subindex].query = loadFloats(query_lh_lc, mname("layers[%d].biases.attention.heads[%d].query", index, subindex));
                    layers[index].biases.attention.heads[subindex].key = loadFloats(key_lh_lc, mname("layers[%d].biases.attention.heads[%d].key", index, subindex));
                    layers[index].biases.attention.heads[subindex].value = loadFloats(value_lh_lc, mname("layers[%d].biases.attention.heads[%d].value", index, subindex));
                }

                attn_o_lc = cJSON_GetObjectItem(attention_lc, "output");
                if (!cJSON_IsArray(attn_o_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].biases.attention.output = loadFloats(attn_o_lc, mname("layers[%d].biases.attention.output", index));

                ffw_lc = cJSON_GetObjectItem(weights_lc, "feed_forward");
                if (!cJSON_IsObject(ffw_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }

                ffw_grow_lc = cJSON_GetObjectItem(ffw_lc, "grow");
                ffw_shrink_lc = cJSON_GetObjectItem(ffw_lc, "shrink");
                if (!cJSON_IsArray(ffw_grow_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                if (!cJSON_IsArray(ffw_shrink_lc)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                layers[index].biases.feed_forward.grow = loadFloats(ffw_grow_lc, mname("layers[%d].biases.feed_forward.grow", index));
                layers[index].biases.feed_forward.shrink = loadFloats(ffw_shrink_lc, mname("layers[%d].biases.feed_forward.shrink", index));
            }
            cJSON* embeddings_raw = cJSON_GetObjectItem(transformer_structure, "embeddings");
            if (!cJSON_IsArray(embeddings_raw)){
                printf("Model file is corrupted.\n");
                return 1;
            }
            int embeddings_raw_size = cJSON_GetArraySize(embeddings_raw);
            if (embeddings_raw_size != vocab_len){
                printf("The model you are trying to load doesn't use the same vocabulary as yours.\n");
                return 1;
            }
            cJSON* curr_embedding_raw_item = cJSON_GetArrayItem(embeddings_raw, 0);

            float** embeddings = calloc((vocab_len + gap_size) * sizeof(float*), 1);
            if (!embeddings){
                printf("Failed to allocate memory to load model.\n");
                return 1;
            }

            char* digits = malloc(11);
            if (!digits){
                printf("Failed memory allocation to load model.\n");
                return 1;
            }
            strcpy(digits, "0123456789");

            for (int index = 0; index < embeddings_raw_size; index++){
                if (!cJSON_IsArray(curr_embedding_raw_item)){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                //get id from filename
                //we will hope id is consistent and the first number in the str of the filename.
                if (!cJSON_IsString(cJSON_GetArrayItem(curr_embedding_raw_item, 0))){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                int curr_embedding_raw_item_filename_len = strlen(cJSON_GetArrayItem(curr_embedding_raw_item, 0)->valuestring);
                if (curr_embedding_raw_item_filename_len == 0){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                char* num_ = malloc(32);
                bool foundid = false;
                
                if (!num_){
                    printf("Failed to allocate memory to load model.\n");
                    return 1;
                }
                int cursor_ = 0;
                for (int subindex = 0; subindex < curr_embedding_raw_item_filename_len; subindex++){
                    char currchr = cJSON_GetArrayItem(curr_embedding_raw_item, 0)->valuestring[subindex];
                    bool isdigit = false;
                    for (int subsubindex = 0; subsubindex < 10; subsubindex++){
                        if (digits[subsubindex] == currchr){
                            isdigit = true;
                            foundid = true;
                            break;
                        }
                    }
                    if (!isdigit){
                        if (foundid){
                            break;
                        }
                    }
                    else{
                        if (cursor_ < 31){
                            num_[cursor_] = currchr;
                            cursor_++;
                        }
                    }
                }
                if (!foundid){
                    printf("Model file is corrupted.\n");
                    return 1;
                }
                num_[cursor_] = '\0';
                int id = atoi(num_);
                free(num_);

                if (!id_to_token(id)){
                    printf("The model you are trying to load doesn't use the same vocabulary as yours.\n");
                    return 1;
                }

                embeddings[id] = loadFloats(curr_embedding_raw_item, mname("embeddings[%d]", id));
                curr_embedding_raw_item = curr_embedding_raw_item->next;
            }

            for (int index = 0; index < n_files; index++){
                if (files[index][0]){
                    free(files[index][0]);
                }
                if (files[index][1]){
                    free(files[index][1]);
                }
                free(files[index]);
            }
            free(files);
            free(files_len);
            printf("Loaded model in %lldms.\n", timer_end(timer_));
        }
    }
    

    printf("Enter strings to tokenize:\n");
    while (true){
        char* in = input("› ");
        int* tokens = tokenize(in);
        
        printf("Token ids: ");
        for (int index = 1; index < tokens[0] + 1; index++){
            printf("%d ", tokens[index]);
        }
        printf("\n");
        printf("Tokens: ");
        for (int index = 1; index < tokens[0] + 1; index++){
            printf("\"%s\" ", id_to_token(tokens[index]));
        }
        printf("\n");
    }

    return 0;
}
