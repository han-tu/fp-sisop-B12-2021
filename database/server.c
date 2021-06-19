#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#define PORT 8080

char* adr="/DB/";
char* tsv=".tsv";
char* baru="new";//buat update sama delete

int server_fd, new_socket, valread;
struct sockaddr_in address;
int opt = 1;
int addrlen = sizeof(address);

char folderDatabase[] = "DB" ;
char currentWorkingDB[255] = {0} ;
int isRoot = 0 ;
int acc = 0 ;

struct loggedin {
    char id[255], password[255] ;
} ;

struct loggedin login ;

// Prototype
void reconnect() ;
int auth(char str[]) ;
char* create_user(char str[]) ;
void write_file(char filePath[], char str[], char mode[]) ;
char* use_db(char str[]) ;
char* grant_permission(char str[]) ;
char* create_db(char str[]) ;
char* create_table(char str[]) ;
char* drop_db(char str[]) ;
void deleteFolderRecursively(char*) ;
char* drop_table(char str[]) ;
int insert(int argc,char* argv[]);
int del(int argc,char* argv[],int mode);
int upd(int argc,char* argv[],int mode);
int selects(int argc,char* argv[], int mode);
int parse(int argc,char* argv[]);
void dml(char str[]) ;
char* getTimestamp() ;
void create_log(char str[]) ;
// ---------

int main() {
    // pid_t pid, sid;
    // pid = fork();

    // if (pid < 0) {
    //     exit(EXIT_FAILURE);
    // }

    // if (pid > 0) {
    //     exit(EXIT_SUCCESS);
    // }

    // umask(0);

    // sid = setsid();
    // if (sid < 0) {
    //     exit(EXIT_FAILURE);
    // }

    // if ((chdir("/")) < 0) {
    //     exit(EXIT_FAILURE);
    // }

    // close(STDIN_FILENO);
    // close(STDOUT_FILENO);
    // close(STDERR_FILENO);

    char buffer[1024] = {0}, msg[1024] = {};
      
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
      
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
      
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    reconnect() ;

    mkdir("DB", 0777) ;
    mkdir("DB/user", 0777) ;
    FILE* file = fopen("DB/user/user.txt", "a") ;
    if(file)
        fclose(file) ;

    strcpy(currentWorkingDB, "...") ;
    int isDML = 0 ;
    while (1) {
        bzero(buffer, 1024) ;
        bzero(msg, 1024) ;
        // Untuk read query (Ex: SELECT * from )
        valread = read(new_socket, buffer, 1024) ;
        if (!valread) {
            acc = isRoot = 0 ;
            reconnect() ;
            continue ;
        }
        printf("-%s\n", buffer) ;
        if (buffer[strlen(buffer) - 1] != ';') {
            strcpy(msg, "SYNTAX ERROR") ;
        }
        else if (!strncmp(buffer, "CREATE USER", 11)) {
            if (isRoot)
                strcpy(msg, create_user(buffer)) ;
            else 
                strcpy(msg, "PERMISSION DENIED") ;
        }
        else if (!strncmp(buffer, "USE", 3)) {
            strcpy(msg, use_db(buffer)) ;
        }
        else if (!strncmp(buffer, "GRANT PERMISSION", 16)) {
            if (isRoot)
                strcpy(msg, grant_permission(buffer)) ;
            else 
                strcpy(msg, "PERMISSION DENIED") ;
        }
        else if (!strncmp(buffer, "CREATE DATABASE", 15)) {
            strcpy(msg, create_db(buffer)) ;
        }
        else if (!strncmp(buffer, "CREATE TABLE", 12)) {

            if (strcmp(currentWorkingDB, "..."))
                strcpy(msg, create_table(buffer)) ;
            else 
                strcpy(msg, "WORKING DATABASE UNDEFINED") ;
        }
        else if (!strncmp(buffer, "DROP DATABASE", 13)) {
            strcpy(msg, drop_db(buffer)) ;
        }
        else if (!strncmp(buffer, "DROP TABLE", 10)) {
            if (strcmp(currentWorkingDB, "..."))
                strcpy(msg, drop_table(buffer)) ;
            else 
                strcpy(msg, "WORKING DATABASE UNDEFINED") ;
        }
        else if (!strncmp(buffer, "SELECT", 6) || !strncmp(buffer, "DELETE", 6) || !strncmp(buffer, "INSERT", 6) || !strncmp(buffer, "UPDATE", 6)) {
            if (strcmp(currentWorkingDB, "...")) {
                isDML = 1 ;
                dml(buffer) ;
            }
            else 
                strcpy(msg, "WORKING DATABASE UNDEFINED") ;
        }
        else {
            strcpy(msg, "QUERY NOT IDENTIFIED") ;
        }
        if (!isDML)
            send(new_socket, msg, strlen(msg), 0) ;
        create_log(buffer) ; 
        isDML = 0 ;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------

void create_log(char str[]) {
    FILE* file = fopen("query.log", "a") ;
    fprintf(file, "%s:%s:%s\n", getTimestamp(), login.id, str) ;
    fclose(file) ;
}

char* getTimestamp() {
    time_t t;
    time(&t);
    struct tm *timeinfo = localtime(&t);

    char timestamp[50] ; bzero(timestamp, 50) ;
    char* ptr ;
    sprintf(timestamp, "%02d-%02d-%02d %d:%02d:%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec );
    ptr = timestamp ;
    return ptr;
}

void dml(char str[]) {
    int argc ;

    char temp[512] ; bzero(temp, 512) ; strcpy(temp, str) ;
    char* token;
    char* rest = temp ;

    int jumlahKata = 0 ;

    int i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        jumlahKata++ ;
    }

    char* argv[jumlahKata] ;
    for (i = 0 ; i < jumlahKata ; i++) {
        argv[i] = (char*) malloc (sizeof(char) * 50) ;
    }

    bzero(temp, 512) ; strcpy(temp, str) ;
    rest = temp ;
    i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (i == jumlahKata - 1) {
            char* semicolon = strchr(token, ';') ;
            char temp2[512] ; bzero(temp2, 512) ;
            strncpy(temp2, token, strlen(token) - strlen(semicolon)) ;
            token = temp2 ;
        }

        strcpy(argv[i], token) ;
        // printf("%s ", argv[i]) ;
        i++ ;
    }
    // printf("\n") ;
    parse (jumlahKata, argv) ;
    
}

char* drop_table(char str[]) {
    char* ptr ; char msg[255] ; bzero(msg, 255) ;
    char table[50] ; bzero(table, 50) ;
    char temp[512] ; bzero(temp, 512) ; strcpy(temp, str) ;
    char* token;
    char* rest = temp ;
    int i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (i == 2) {
            strncpy(table, token, strlen(token) - 1) ;
        }
        else if (i > 2) {
            strcpy(msg, "SYNTAX ERROR") ;
            ptr = msg ;
            return ptr ;
        }
        i++ ;
    }

    char path[512] ; bzero(path, 512) ;
    sprintf(path, "%s/%s/%s.csv", folderDatabase, currentWorkingDB, table) ;

    FILE* file = fopen(path, "r") ;
    if (!file) {
        strcpy(msg, "TABLE NOT FOUND") ;
        ptr = msg ;
        return ptr ;
    }

    unlink(path) ;

    strcpy(msg, "DROP TABLE SUCCESS") ;
    ptr = msg ;
    return ptr ;
}

char* drop_db(char str[]) {
    char* ptr ; char msg[255] ; bzero(msg, 255) ;
    char db[50] ; bzero(db, 50) ;
    char temp[512] ; bzero(temp, 512) ; strcpy(temp, str) ;
    char* token;
    char* rest = temp ;
    int i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (i == 2) {
            strncpy(db, token, strlen(token) - 1) ;
        }
        else if (i > 2) {
            strcpy(msg, "SYNTAX ERROR") ;
            ptr = msg ;
            return ptr ;
        }
        i++ ;
    }

    char path[255] ; bzero(path, 255) ;
    sprintf(path, "%s/%s/granted_user.txt", folderDatabase, db) ;

    FILE* file = fopen(path, "r") ;
    if (!file) {
        strcpy(msg, "DATABASE NOT FOUND") ;
        ptr = msg ;
        return ptr ;
    }

    char line[80] ; int isUser = 0 ;
    while(fgets(line, 80, file)) {
        if (!strncmp(line, login.id, strlen(login.id))) {
            isUser = 1 ;
            break ;
        }
    }
    fclose(file) ;

    if (!isUser) {
        strcpy(msg, "PERMISSION DENIED") ;
        ptr = msg ;
        return ptr ;
    }

    bzero(path, 255) ;
    sprintf(path, "%s/%s", folderDatabase, db) ;

    deleteFolderRecursively(path) ;
    rmdir(path) ;

    strcpy(msg, "DROP DATABASE SUCCESS") ;
    ptr = msg ;
    return ptr ;
}

void deleteFolderRecursively(char *basePath)
{
    char path[1000];
    struct dirent *dp;
    DIR *dir = opendir(basePath);
 
    if (!dir)
        return;
 
    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);
            if (dp->d_type & DT_DIR) {
                deleteFolderRecursively(path);
                rmdir(path) ;
            }
            else
                unlink(path) ;
        }
    }
    closedir(dir);
}

char* create_table(char str[]) {
    char* ptr ; char msg[255] ; bzero(msg, 255) ;
    char table[50] ; bzero(table, 50) ;
    char temp[512] ; bzero(temp, 512) ; strcpy(temp, str) ;
    char* token;
    char* rest = temp ;
    int i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (i == 2) {
            strcpy(table, token) ;
            if (!strchr(str, '(')) {
                strcpy(msg, "SYNTAX ERROR") ;
                ptr = msg ;
                return ptr ;
            }
            break ;
        }
        i++ ;
    }
    char path[512] ; bzero(path, 512) ;
    sprintf(path, "%s/%s/%s.tsv", folderDatabase, currentWorkingDB, table) ;
    FILE* file = fopen(path, "r") ;
    if (file) {
        fclose(file) ;
        strcpy(msg, "TABLE ALREADY EXIST") ;
        ptr = msg ; return ptr ;
    }
    
    bzero(temp, 512) ;
    strcpy(temp, str) ;
    char header[255] ; bzero(header, 255) ;

    char* kurungbuka = strchr(temp, '(') + 1 ;

    rest = kurungbuka ;
    i = 0 ;
    while (token = strtok_r(rest, ",", &rest)) {
        if (i != 0) token++ ;

        if (strchr(token, ')')) {
            char* tutup = strchr(token, ')') ;
            strncpy(token, token, strlen(token) - strlen(tutup)) ;
        }
        char* space = strchr(token, ' ') ;
        if (i != 0)
            strcat(header, "\t") ;
        if (space)
            strncat(header, token, strlen(token) - strlen(space)) ;
        else 
            strcat(header, token) ;
        i++ ;
    }

    file = fopen(path, "a") ;
    fprintf(file, "%s\n", header) ;
    fclose(file) ;

    strcpy(msg, "TABLE CREATED SUCCESSFULLY") ;
    ptr = msg ;
    return ptr ;
}

char* create_db(char str[]) {
    char* ptr ; char msg[255] ; bzero(msg, 255) ;
    char db[50] ; bzero(db, 50) ;
    char temp[512] ; bzero(temp, 512) ; strcpy(temp, str) ;
    char* token;
    char* rest = temp ;
    int i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (i == 2) {
            strncpy(db, token, strlen(token) - 1) ;
        }
        else if (i > 2) {
            strcpy(msg, "SYNTAX ERROR") ;
            ptr = msg ;
            return ptr ;
        }
        i++ ;
    }

    char path[100] ; bzero(path, 100) ;
    sprintf(path, "DB/%s", db) ;
    printf("%s\n", path) ;
    DIR* dir = opendir(path) ;
    if (dir) {
        strcpy(msg, "DATABASE ALREADY EXIST") ;
        ptr = msg ;
        closedir(dir) ;
        return ptr ;
    }
    
    mkdir(path, 0777) ;

    strcat(path, "/granted_user.txt") ;
    FILE *file = fopen(path, "a") ;
    if (!isRoot)
        fprintf(file, "%s\n", login.id) ;
    fclose(file);

    strcpy(msg, "DATABASE CREATED SUCCESSFULLY") ;
    ptr = msg ;
    return ptr ;

}

char* grant_permission(char str[]) {
    char* ptr ;
    char db[50], userid[50], msg[255] ;
    bzero(db, 50) ; bzero(userid, 50); bzero(msg, 255) ;
    char temp[512] ; bzero(temp, 512) ; strcpy(temp, str) ;
    char* token;
    char* rest = temp;
    int i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (i == 4) {
            strncpy(db, token, strlen(token) - 1) ;
        }
        else if (i == 2) {
            strcpy(userid, token) ;
        }
        else if ((i == 3 && strcmp(token, "INTO")) || i > 4) {
            strcpy(msg, "SYNTAX ERROR") ;
            ptr = msg ;
            return ptr ;
        }
        i++ ;
    }
    printf("id:%s\n", userid) ;
    printf("db:%s\n", db) ;
    FILE* file = fopen("DB/user/user.txt", "r") ;
    
    int isUser = 0 ;
    char line[80] ; 
    while(fgets(line, 80, file)) {
        if (!strncmp(line, userid, strlen(userid))) {
            isUser = 1 ;
            break ;
        }
    }
    fclose(file) ;

    if (!isUser) {
        strcpy(msg, "USER NOT FOUND") ;
        ptr = msg ;
        return ptr ;
    }

    char fileName[255] ;
    sprintf(fileName, "%s/%s/granted_user.txt", folderDatabase, db) ;
    printf("%s\n", fileName) ;
    file = fopen(fileName, "a") ;
    if (!file) { // Jika database tidak ada
        strcpy(msg, "DATABASE NOT FOUND") ;
        ptr = msg ;
        return ptr ;
    }
    fprintf(file, "%s\n", userid) ;
    fclose(file) ;

    strcpy(msg, "ACCESS GRANTED") ;
    ptr = msg ; return ptr ;
}

char* use_db(char str[]) {
    char msg[255] ; bzero(msg, 255) ;
    char* temp = str + 4 ;
    char db[50] ; bzero(db, 50) ; strncpy(db, temp, strlen(temp) - 1) ;
    // printf("%s\n", db) ;

    char fileName[100] ;
    bzero(fileName, 100) ;
    sprintf(fileName, "%s/%s/granted_user.txt", folderDatabase, db) ;

    FILE* file = fopen(fileName, "r") ;
    if (!file) { // Jika database tidak ada
        strcpy(msg, "DATABASE NOT FOUND") ;
        temp = msg ;
        return temp ;
    }

    char line[80] ;
    while(fgets(line, 80, file)) {
        if (!strncmp(line, login.id, strlen(login.id))) {
            fclose(file) ;
            strcpy(msg, "WORKING DATABASE CHANGED") ;
            temp = msg ;
            strcpy(currentWorkingDB, db) ;
            return temp ;
        }
    }
    fclose(file) ;
    strcpy(msg, "PERMISSION DENIED") ;
    temp = msg ;
    return temp ;
}

char* create_user(char str[]) {
    char* ptr ; char msg[255] ; bzero(msg, 255) ;
    char userid[255], password[255] ;
    bzero(userid, 255); bzero(password, 255) ;
    char temp[512] ; bzero(temp, 512) ; strcpy(temp, str) ;
    char* token;
    char* rest = temp;
    int i = 0 ;

    while ((token = strtok_r(rest, " ", &rest))) {
        if (i == 2) {
            strcpy(userid, token) ;
        }
        else if ((i == 3 && strcmp(token, "IDENTIFIED")) || (i == 4 && strcmp(token, "BY")) || i > 5) {
            strcpy(msg, "SYNTAX ERROR") ;
            ptr = msg ;
            return ptr ;
        }
        else if (i == 5) {
            strncpy(password, token, strlen(token) - 1) ;
        }
        i++ ;
    }

    bzero(temp, 512) ;
    sprintf(temp, "%s:%s", userid, password) ;
    write_file("DB/user/user.txt", temp, "a") ;
    strcpy(msg, "USER CREATED SUCCESSFULLY") ;
    ptr = msg ;
    return ptr ;
}

void write_file(char fileName[], char str[], char mode[]) {
    FILE* file = fopen(fileName, mode) ;
    fprintf(file, "%s\n", str) ;
    fclose(file) ;
}

void reconnect() {
    char buffer[1024] = {0}, msg[1024] = {0} ;
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // Terima UID
    valread = read(new_socket, buffer, 1024) ;

    if (!strcmp(buffer, "0")) {// Apakah dia superuser
        strcpy(msg, "Connection accepted. Welcome, root!") ;
        acc = 1 ;
        isRoot = 1 ;
        strcpy(login.id, "root") ;
    }

    if (!acc) {
        send(new_socket, "anjay", 10, 0) ;  // Cuma untuk ngehandle
        bzero(buffer, 1024) ;               // send 2x di client

        valread = read(new_socket, buffer, 1024) ; // Terima id password

        if (auth(buffer)) {
            acc = 1 ;
            strcpy(msg, "Connection accepted!") ;

            // Menyimpan siapa yang sedang login
            char* ptr = buffer ;
            char* separator = strchr(buffer, ':') + 1 ;
            strcpy(login.password, separator) ;
            strncpy(login.id, ptr, strlen(ptr) - strlen(separator) - 1) ;
            // ----
        }
        else {
            strcpy(msg, "Username or password is invalid") ;
        }
    }
    send(new_socket, msg, strlen(msg), 0) ;
}

int auth(char str[]) {
    char fileName[512] ;
    sprintf(fileName, "%s/user/user.txt", folderDatabase) ;
    FILE* file = fopen(fileName, "r");
    char line[80] ;
    while(fgets(line, 80, file)) {
        if (!strcmp(line, str)) {
            fclose(file) ;
            return 1 ;
        }
    }
    fclose(file) ;
    return 0 ;
}

// -- DML

int insert(int argc,char* argv[]){
    char adrtmp[512];
    bzero(adrtmp,512);
    sprintf(adrtmp, "%s/%s/%s.tsv", folderDatabase, currentWorkingDB, argv[2]) ;

    FILE* find=fopen(adrtmp,"r");//cek file ada gak
        if(find==NULL){
            char msg[255] ; bzero(msg, 255) ;
            strcpy(msg, "TABLE NOT FOUND") ;
            send(new_socket, msg, strlen(msg), 0) ;
            return 0;
        }
    fclose(find);
    
    char line[100];
    int colcount=0;
    FILE* stream=fopen(adrtmp,"r");
    fgets(line,1024,stream);
    fclose(stream);
    for(int i=0;i<strlen(line)-2;i++)
        if(line[i]=='\t')colcount++;
        
    int incount=1;
    for(int i=0;i<strlen(argv[3]);i++)
        if(argv[3][i]==',')incount++;
        
    if(incount!=colcount+1){
    // printf("%d %d\n",incount,colcount);
        // printf("number of inputs does not match number of columns\n");
        char msg[255] ; bzero(msg, 255) ;
            strcpy(msg, "NUMBER OF INPUTS DOESN'T MATCH WITH NUMBER OF COLUMNS") ;
            send(new_socket, msg, strlen(msg), 0) ;
        return 0;
    }
        
    FILE*  in=fopen(adrtmp,"a");
    char* input=strtok(argv[3],"()");//hapus kurung
    char* token=NULL;
    token=strtok(input,",");

    while(token!=NULL){
        fputs(token,in);
        fprintf(in,"\t");
        token=strtok(NULL,",");
    }

    fprintf(in,"\n");
    fclose(in);

    char msg[255] ; bzero(msg, 255) ;
    strcpy(msg, "INSERT SUCCESS") ;
    send(new_socket, msg, strlen(msg), 0) ;
}

int del(int argc,char* argv[],int mode){
    char adrtmp1[512];//file lama
    char adrtmp2[512];//file baru
    bzero(adrtmp1,512);
    bzero(adrtmp2,512);
    sprintf(adrtmp1, "%s/%s/%s.tsv", folderDatabase, currentWorkingDB, argv[2]) ;
    sprintf(adrtmp2, "%s/%s/%snew.tsv", folderDatabase, currentWorkingDB, argv[2]) ;
    
    FILE* stream=fopen(adrtmp1,"r");//file ada gak
        if(stream==NULL){
        char msg[255] ; bzero(msg, 255) ;
        strcpy(msg, "TABLE NOT FOUND") ;
        send(new_socket, msg, strlen(msg), 0) ;
        return 0;
        }
    fclose(stream);

    if(mode==0){//gak ada where
    char line[1024];
    FILE* kolom=fopen(adrtmp1,"r");
    fgets(line,1024,kolom);
    fclose(kolom);
    remove(adrtmp1);

    FILE* create=fopen(adrtmp1,"a");
    fputs(line,create);
    fclose(create);
    }

    if(mode==1){//ada where
    char condition1[36];
    char condition2[36];
    char full[72];
    
    char* token=NULL;
    strcpy(full,argv[4]);
    token=strtok(full,"=");
    strcpy(condition1,token);
    token=strtok(NULL,"=");
    strcpy(condition2,token);

    char line[100];
    int colcount=0;
    FILE* stream=fopen(adrtmp1,"r");
    fgets(line,1024,stream);
    fclose(stream);
    for(int i=0;i<strlen(line);i++)
        {
        if(line[i]=='\t')colcount++;
        }
    token=strtok(line,"\t");
    int index;
    if(strcmp(condition1,token)==0)index=0;
    else
    for(int i=1;i<=colcount;i++){
        if(i==colcount)token=strtok(NULL,"\n");
        else token=strtok(NULL,"\t");
        if(strcmp(condition1,token)==0){
        // printf("%s %s\n",condition1,token);
        index=i;
        break;
        }
        if(strcmp(condition1,token)!=0&&i==colcount){
            char msg[255] ; bzero(msg, 255) ;
            strcpy(msg, "COLUMN NOT FOUND NEAR WHERE") ;
            send(new_socket, msg, strlen(msg), 0) ;
        return 0;
        }
        }

        FILE* old=fopen(adrtmp1,"r");
        FILE* neww=fopen(adrtmp2,"a");
        char firstl[100];
        char copy[100];
        int rownum=0,flag;

        while(fgets(firstl,100,old)!=NULL){
            if(rownum==0){
            fputs(firstl,neww);
            rownum++;
            continue;
            }
            flag=1;
            char* token2=NULL;
            strcpy(copy,firstl);
            token2=strtok(copy,"\t");
            if(index==0){
                if(strcmp(token2,condition2)==0)
                    flag=0;
            }else
            for(int i=1;i<=colcount;i++){
                token2=strtok(NULL,"\t");
                if(i==index)
                    if(strcmp(token2,condition2)==0)
                        flag=0;
                        }
                if(flag==1)fputs(firstl,neww);
                }

        fclose(old);
        fclose(neww);
        remove(adrtmp1);
        rename(adrtmp2,adrtmp1);
        }
    bzero(adrtmp1,100);
    bzero(adrtmp2,100);

    char msg[255] ; bzero(msg, 255) ;
    strcpy(msg, "DELETE SUCCESS") ;
    send(new_socket, msg, strlen(msg), 0) ;
}
// tmp1 = namafile.tsv
// tmp2 = namafilenew.tsv
int upd(int argc,char* argv[],int mode){
    char adrtmp1[512];//file lama 
    char adrtmp2[512];//file baru
    bzero(adrtmp1, 512) ; bzero(adrtmp2, 512) ;
    sprintf(adrtmp1, "%s/%s/%s.tsv", folderDatabase, currentWorkingDB, argv[1]) ;
    sprintf(adrtmp2, "%s/%s/%snew.tsv", folderDatabase, currentWorkingDB, argv[1]) ;
    FILE* stream=fopen(adrtmp1,"r");
    if(stream==NULL){
        char msg[255] ; bzero(msg, 255) ;
        strcpy(msg, "TABLE NOT FOUND") ;
        send(new_socket, msg, strlen(msg), 0) ;
        return 0;
    }
    fclose(stream);
    
    char set1[36];//kolom
    char set2[36];//value
    char* token=NULL;
    token=strtok(argv[3],"=");
    strcpy(set1,token);
    token=strtok(NULL,"=");
    strcpy(set2,token);
    
    char line[100];
    char copy[100];
    int colcount=0;
    FILE* getline=fopen(adrtmp1,"r");
    fgets(line,100,getline);
    fclose(getline);
    
    for(int i=0;i<strlen(line);i++)
        if(line[i]=='\t')colcount++;
        
    strcpy(copy,line);    
    token=strtok(copy,"\t");
    int indexset;
    if(strcmp(set1,token)==0)indexset=0;
    else
    for(int i=1;i<=colcount;i++){
        if(i==colcount)
            token=strtok(NULL,"\n");
        else
            token=strtok(NULL,"\t");

        if(strcmp(set1,token)==0){
            indexset=i;
            break;
        }
        if(strcmp(set1,token)!=0&&i==colcount){
            char msg[255] ; bzero(msg, 255) ;
            strcpy(msg, "COLUMN NOT FOUND NEAR SET") ;
            send(new_socket, msg, strlen(msg), 0) ;
            return 0;
        }
    }    

    if(mode==0){//gak ada where
    
    FILE* old=fopen(adrtmp1,"r");
    FILE* neww=fopen(adrtmp2,"a");
    char firstl[100];
    int rownum=0,flag;
    while(fgets(firstl,100,old)!=NULL){
        if(rownum==0){
            fputs(firstl,neww);
            rownum++;
            continue;
        }
        char* token2=NULL;
        char copy2[100];
        token2=strtok(firstl,"\t");
        if(indexset==0){
            strcat(copy2,set2);
            strcat(copy2,"\t");
        }
        else {
            strcat(copy2,token2);
            strcat(copy2,"\t");
        }

    for(int i=1;i<=colcount;i++){
        token2=strtok(NULL,"\t");
        if(i==indexset){
                strcat(copy2,set2);
                strcat(copy2,"\t");
            }
        else {
            strcat(copy2,token2);
            strcat(copy2,"\t");
            }
        }
        strcat(copy2,"\n");
        //printf("%s\n",copy2);
        fputs(copy2,neww);
        bzero(copy2,100);
    }

        fclose(old);
        fclose(neww);
        remove(adrtmp1);
        rename(adrtmp2,adrtmp1);
    }

    if(mode==1){//ada where
    char condition1[36];//kondisi where kiri
    char condition2[35];//kondisi where kanan
    char* token=NULL;

    token=strtok(argv[5],"=");
    strcpy(condition1,token);
    token=strtok(NULL,"=");
    strcpy(condition2,token);
    
    char* token2=NULL;
    token2=strtok(line,"\t");
    int indexcon;
    if(strcmp(condition1,token2)==0)indexcon=0;//nyari indeks kolom yang sama dengan kondisi 1
    else
    for(int i=1;i<=colcount;i++){
        if(i==colcount)
            token2=strtok(NULL,"\n");
        else
            token2=strtok(NULL,"\t");

        if(strcmp(condition1,token2)==0){
            indexcon=i;
            break;
                }
        if(strcmp(condition1,token2)!=0&&i==colcount){
            char msg[255] ; bzero(msg, 255) ;
            strcpy(msg, "COLUMN NOT FOUND NEAR WHERE") ;
            send(new_socket, msg, strlen(msg), 0) ;
            return 0;
                }
            }

    FILE* old=fopen(adrtmp1,"r");
    FILE* neww=fopen(adrtmp2,"a");
    int rownum=0,flag;
    char firstl[100];
    while(fgets(firstl,100,old)!=NULL){
        if(rownum==0){
        fputs(firstl,neww);
        rownum++;
        continue;
        }
        flag=1;//1 gak di set, 0 di set
        char* token2=NULL;
        char copy2[100];
        strcpy(copy2,firstl);
        token2=strtok(copy2,"\t");
        if(indexcon==0){
            if(strcmp(token2,condition2)==0)
                flag=0;
        }else
        for(int i=1;i<=colcount;i++){
            token2=strtok(NULL,"\t");
            if(i==indexcon)
                if(strcmp(token2,condition2)==0)
                    flag=0;
                }
            if(flag==1)fputs(firstl,neww);//gak di set
            if(flag==0){//di set
                char* token3=NULL;
                char copy3[100];
                token3=strtok(firstl,"\t");
                if(indexset==0){
                    strcat(copy3,set2);
                    strcat(copy3,"\t");
                }
                else {
                    strcat(copy3,token3);
                    strcat(copy3,"\t");
                }

                for(int i=1;i<=colcount;i++){
                    token3=strtok(NULL,"\t");
                    if(i==indexset){
                            strcat(copy3,set2);
                            strcat(copy3,"\t");
                        }
                    else {
                        strcat(copy3,token3);
                        strcat(copy3,"\t");
                        }
                    }
                    strcat(copy3,"\n");
                    fputs(copy3,neww);
                    bzero(copy3,100);
            }
        }
        fclose(old);
        fclose(neww);
        remove(adrtmp1);
        rename(adrtmp2,adrtmp1);
        }
	bzero(adrtmp1,512);
	bzero(adrtmp2,512);

    char msgs[50] ; bzero(msgs, 50) ;
    strcpy(msgs, "UPDATE SUCCESS") ;
    send(new_socket, msgs, strlen(msgs), 0) ;
}

int selects(int argc,char* argv[], int mode){
    char adrtmp[512]; bzero(adrtmp, 512) ;
    sprintf(adrtmp, "%s/%s/%s.tsv", folderDatabase, currentWorkingDB, argv[3]) ;
    
    char line[100];
    FILE* stream=fopen(adrtmp,"r");//file ada gak
        if(stream==NULL){
        char msg[255] ; bzero(msg, 255) ;
        strcpy(msg, "TABLE NOT FOUND") ;
        send(new_socket, msg, strlen(msg), 0) ;
        return 0;
        }
    int colcount=0;
    fgets(line,100,stream);
    for(int i=0;i<strlen(line);i++)
        if(line[i]=='\t')colcount++;    
    fclose(stream);
    
    char sender[1024] ; bzero(sender, 1024) ;
    if(mode==1){//bintang sama gak ada where
        FILE* open=fopen(adrtmp,"r");
        char line[100];
        while(fgets(line,100,open)!=NULL){
            strcat(sender, line) ;
        }
        send(new_socket, sender,strlen(sender), 0) ;
        fclose(open) ;
    }
        
    if(mode==2){//bintang sama ada where
        char condition1[36];
        char condition2[36];
        char* token=NULL;
        token=strtok(argv[5],"=");
        strcpy(condition1,token);
        token=strtok(NULL,"=");
        strcpy(condition2,token);

        char copy[100];
        strcpy(copy,line);
        token=strtok(copy,"\t");
        int index;
        if(strcmp(condition1,token)==0)index=0;
        else
        for(int i=1;i<=colcount;i++){
            if(i==colcount)
                token=strtok(NULL,"\n");
            else
                token=strtok(NULL,"\t");

            if(strcmp(condition1,token)==0){
                index=i;
                break;
                }
            if(strcmp(condition1,token)!=0&&i==colcount){
                char msg[255] ; bzero(msg, 255) ;
                strcpy(msg, "COLUMN NOT FOUND") ;
                send(new_socket, msg, strlen(msg), 0) ;
                return 0;
                }
            }

        FILE* open=fopen(adrtmp,"r");
        char firstl[100];
        char copy2[100];
        int rownum=0,flag;
        while(fgets(firstl,100,open)!=NULL){
            if(rownum==0){
                strcat(sender, firstl) ;
                rownum++;
                continue;
            }
            flag=1;
            char* token2=NULL;
            strcpy(copy2,firstl);
            token2=strtok(copy2,"\t");
            if(index==0){
                if(strcmp(token2,condition2)==0)
                    flag=0;
            }else
            for(int i=1;i<=colcount;i++){
                token2=strtok(NULL,"\t");
                if(i==index)
                    if(strcmp(token2,condition2)==0)
                        flag=0;
                        }
                if(flag==0)strcat(sender,firstl);
                }
        fclose(open);

        send(new_socket, sender, strlen(sender), 0) ;
    }
    if(mode==3){
        char copy3[100];
        
        

        char* token=NULL;
        int indexes[20];
        token=strtok(argv[2],",");
        while(token!=NULL){
        bzero(copy3,100);
            strcpy(copy3,line);
            token=strtok(NULL,",");    
        }
    }
    
    if(mode==4){
        // ---
    }
}

int parse(int argc,char* argv[]){
	if(strcmp(argv[0],"INSERT")==0)
	insert(argc,argv);

	else if(strcmp(argv[0],"SELECT")==0){
	int flagwhere=0,flagstar=0;
    	if(strcmp(argv[1],"*")==0)flagstar=1;
	if(argc>5)flagwhere=1;
	if(flagstar==1&&flagwhere==1)selects(argc,argv,2);
	else if(flagstar==1&&flagwhere==0)selects(argc,argv,1);
	}

	else if(strcmp(argv[0],"DELETE")==0){
	if(argc>4)//ada where apa gak
        	del(argc,argv,1);
	else
    	del(argc,argv,0);
	}

	else if(strcmp(argv[0],"UPDATE")==0){
	if(argc>5)//ada where apa gak
    	upd(argc,argv,1);
	else
    	upd(argc,argv,0);
	}

}