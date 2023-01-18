#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[])
{
    int num[] = {4, 2, 3, 5, 1, 4, 4, 1, 1, 1, 1, 2, 5, 4, 1, 5, 4, 3, 4, 1, 3, 2, 3, 2, 1, 5, 5, 3, 2, 5, 4, 5, 1, 3, 5, 3, 4, 4, 5, 2, 5, 1, 3, 3, 5, 2, 4, 2, 4, 5, 5, 2, 4, 5, 2, 1, 2, 2, 1, 3, 4, 5, 5, 1, 5, 3, 3, 2, 4, 1, 1, 3, 2, 5, 5, 5, 4, 5, 5, 1, 2, 1, 1, 3, 2, 4, 1, 2, 3, 2, 1, 2, 3, 4, 5, 1, 4, 4, 3, 4, 3, 4, 4, 5, 1, 1, 3, 1, 4, 3, 3, 4, 4, 1, 2, 2, 4, 4, 3, 3, 3, 3, 3, 2, 4, 3, 2, 3, 5, 4, 1, 3, 4, 2, 3, 2, 5, 4, 2, 2, 4, 3, 5, 4, 3, 2, 3, 4, 3, 2, 4, 2, 2, 4, 5, 5, 1, 1, 1, 3, 2, 3, 4, 2, 4, 2, 1, 2, 5, 5, 5, 5, 5, 5, 1, 3, 5, 2, 3, 1, 3, 4, 1, 1, 1, 1, 5, 4, 5, 4, 4, 1, 3, 3, 1, 2, 2, 1, 4, 2};
    //由python生成的随机数队列（不能用）
    int x = 100; //进行录入的次数
    int randomnum[x];
    for (int i = 0; i < x; i++){
        randomnum[i] = num[i];
    }
    int fd;
    fd=open("iotest1", O_CREATE);
    close(fd);
    fd=open("iotest2", O_CREATE);
    close(fd);
    fd=open("iotest3", O_CREATE);
    close(fd);
    fd=open("iotest4", O_CREATE);
    close(fd);
    fd=open("iotest5", O_CREATE);
    close(fd);
    int stick=uptime();
    for (int i = 0; i < x; i++)    //循环随机数队列
    {
        switch(randomnum[i])
        {
            case 1:{
                int fd1=open("iotest1", O_RDWR);
                write(fd1, "9c0DDfIRXlyRsUiu4vx402GbLmFrxOGcAzNtKCW3pXk0yVwiugbH3ieuO9bDx98cgsTEqC5aZMGO3mHNA7o7LfBmLlahpqHWXClEkLpFPpNZfoDWj366XzwdTUzfy8reqHMob6k3CfJHcvE2LD7poDpmCTYjLSDn0BwOrZmwYXnm0i6vyOC2vBaQ9isc60eYLQhDukesXgQL6KQaTu9R9BCSlaDtuTtuiOGJqKvLdV3fugVzV6apLbEr5fgHjuj8UWx8lRxhufIVBfIEo9kxuKgtI0EKDNQM7P70aQVAbCNnF3KpQppnfGaph0o369wRZEzcdxrpo7nC71bx9FmrqQCPp0ot5UJfdXi9WA5bnrslFGHacCdrH8gZ3Touki0IpzjGd9WPdOJWVCvPiR9VdJVvV4f8sieHpIY24kwR8lF2UiPfSHaqtZNQZbBaWlsv6EwOlHRngdBQoQm24zeX3mN6z09wI6cn4j3jcVZ4PKVamX8dQPuETlPR9zpgdjF43JDbuXNDgXarkCmn5JdiL6BaH3XoF2DMoqo1EHwpT91R0tsKndfsB1P7HEic8hXkv27IJTV3ZmXAeEanBlLx9vKqETWU1ya78c2Yp5pW7syU7z8hUCyhPOUAnpS4GA2JuXTpYzVeMyZ8TNTd7YVvhfLOIMbUbIepPntjX4C85MYbWuQO79uEaCxmtS9ehOcmknO5R8thOL6PlFnCL7yCZGmlSHF7iT9wvEqNXpv9DN1qIIKoksOo8Wfolo0x8MXZyXcnV8rWr55E4vVkIxORhAVbvEgNGu0JyzQ78OBvO0MV0JO1xatbGRf3Qq6FraZrL9dzw1Ymt1SBndbi1CGBzexlI7TL6qe956Pn6Mq5jxGpgybAGoYKMyWEzGiUQYFEC2gS4lAiB5xT7TnbADrBAd4I119LtZF8lDEGNpzbmTa2BMcbq7mDBd4ELJQpnwq9cG25o1xhjhJM1yfCCEipbVMJrc0intY4AZRXQ7dnNT3AP3vV9pOD24MRESeL7cux", 1024);
                printf("文件标识符%d表示的iotest1已被写入\n",fd1);
                close(fd1);
                break;
            }
            case 2:{
                int fd1=open("iotest2", O_RDWR);
                write(fd1, "9c0DDfIRXlyRsUiu4vx402GbLmFrxOGcAzNtKCW3pXk0yVwiugbH3ieuO9bDx98cgsTEqC5aZMGO3mHNA7o7LfBmLlahpqHWXClEkLpFPpNZfoDWj366XzwdTUzfy8reqHMob6k3CfJHcvE2LD7poDpmCTYjLSDn0BwOrZmwYXnm0i6vyOC2vBaQ9isc60eYLQhDukesXgQL6KQaTu9R9BCSlaDtuTtuiOGJqKvLdV3fugVzV6apLbEr5fgHjuj8UWx8lRxhufIVBfIEo9kxuKgtI0EKDNQM7P70aQVAbCNnF3KpQppnfGaph0o369wRZEzcdxrpo7nC71bx9FmrqQCPp0ot5UJfdXi9WA5bnrslFGHacCdrH8gZ3Touki0IpzjGd9WPdOJWVCvPiR9VdJVvV4f8sieHpIY24kwR8lF2UiPfSHaqtZNQZbBaWlsv6EwOlHRngdBQoQm24zeX3mN6z09wI6cn4j3jcVZ4PKVamX8dQPuETlPR9zpgdjF43JDbuXNDgXarkCmn5JdiL6BaH3XoF2DMoqo1EHwpT91R0tsKndfsB1P7HEic8hXkv27IJTV3ZmXAeEanBlLx9vKqETWU1ya78c2Yp5pW7syU7z8hUCyhPOUAnpS4GA2JuXTpYzVeMyZ8TNTd7YVvhfLOIMbUbIepPntjX4C85MYbWuQO79uEaCxmtS9ehOcmknO5R8thOL6PlFnCL7yCZGmlSHF7iT9wvEqNXpv9DN1qIIKoksOo8Wfolo0x8MXZyXcnV8rWr55E4vVkIxORhAVbvEgNGu0JyzQ78OBvO0MV0JO1xatbGRf3Qq6FraZrL9dzw1Ymt1SBndbi1CGBzexlI7TL6qe956Pn6Mq5jxGpgybAGoYKMyWEzGiUQYFEC2gS4lAiB5xT7TnbADrBAd4I119LtZF8lDEGNpzbmTa2BMcbq7mDBd4ELJQpnwq9cG25o1xhjhJM1yfCCEipbVMJrc0intY4AZRXQ7dnNT3AP3vV9pOD24MRESeL7cux", 1024);
                printf("文件标识符%d表示的iotest2已被写入\n",fd1);
                close(fd1);
                break;
            }
            case 3:{
                int fd1=open("iotest3", O_RDWR);
                write(fd1, "9c0DDfIRXlyRsUiu4vx402GbLmFrxOGcAzNtKCW3pXk0yVwiugbH3ieuO9bDx98cgsTEqC5aZMGO3mHNA7o7LfBmLlahpqHWXClEkLpFPpNZfoDWj366XzwdTUzfy8reqHMob6k3CfJHcvE2LD7poDpmCTYjLSDn0BwOrZmwYXnm0i6vyOC2vBaQ9isc60eYLQhDukesXgQL6KQaTu9R9BCSlaDtuTtuiOGJqKvLdV3fugVzV6apLbEr5fgHjuj8UWx8lRxhufIVBfIEo9kxuKgtI0EKDNQM7P70aQVAbCNnF3KpQppnfGaph0o369wRZEzcdxrpo7nC71bx9FmrqQCPp0ot5UJfdXi9WA5bnrslFGHacCdrH8gZ3Touki0IpzjGd9WPdOJWVCvPiR9VdJVvV4f8sieHpIY24kwR8lF2UiPfSHaqtZNQZbBaWlsv6EwOlHRngdBQoQm24zeX3mN6z09wI6cn4j3jcVZ4PKVamX8dQPuETlPR9zpgdjF43JDbuXNDgXarkCmn5JdiL6BaH3XoF2DMoqo1EHwpT91R0tsKndfsB1P7HEic8hXkv27IJTV3ZmXAeEanBlLx9vKqETWU1ya78c2Yp5pW7syU7z8hUCyhPOUAnpS4GA2JuXTpYzVeMyZ8TNTd7YVvhfLOIMbUbIepPntjX4C85MYbWuQO79uEaCxmtS9ehOcmknO5R8thOL6PlFnCL7yCZGmlSHF7iT9wvEqNXpv9DN1qIIKoksOo8Wfolo0x8MXZyXcnV8rWr55E4vVkIxORhAVbvEgNGu0JyzQ78OBvO0MV0JO1xatbGRf3Qq6FraZrL9dzw1Ymt1SBndbi1CGBzexlI7TL6qe956Pn6Mq5jxGpgybAGoYKMyWEzGiUQYFEC2gS4lAiB5xT7TnbADrBAd4I119LtZF8lDEGNpzbmTa2BMcbq7mDBd4ELJQpnwq9cG25o1xhjhJM1yfCCEipbVMJrc0intY4AZRXQ7dnNT3AP3vV9pOD24MRESeL7cux", 1024);
                printf("文件标识符%d表示的iotest3已被写入\n",fd1);
                close(fd1);
                break;
            }
            case 4:{
                int fd1=open("iotest4", O_RDWR);
                write(fd1, "9c0DDfIRXlyRsUiu4vx402GbLmFrxOGcAzNtKCW3pXk0yVwiugbH3ieuO9bDx98cgsTEqC5aZMGO3mHNA7o7LfBmLlahpqHWXClEkLpFPpNZfoDWj366XzwdTUzfy8reqHMob6k3CfJHcvE2LD7poDpmCTYjLSDn0BwOrZmwYXnm0i6vyOC2vBaQ9isc60eYLQhDukesXgQL6KQaTu9R9BCSlaDtuTtuiOGJqKvLdV3fugVzV6apLbEr5fgHjuj8UWx8lRxhufIVBfIEo9kxuKgtI0EKDNQM7P70aQVAbCNnF3KpQppnfGaph0o369wRZEzcdxrpo7nC71bx9FmrqQCPp0ot5UJfdXi9WA5bnrslFGHacCdrH8gZ3Touki0IpzjGd9WPdOJWVCvPiR9VdJVvV4f8sieHpIY24kwR8lF2UiPfSHaqtZNQZbBaWlsv6EwOlHRngdBQoQm24zeX3mN6z09wI6cn4j3jcVZ4PKVamX8dQPuETlPR9zpgdjF43JDbuXNDgXarkCmn5JdiL6BaH3XoF2DMoqo1EHwpT91R0tsKndfsB1P7HEic8hXkv27IJTV3ZmXAeEanBlLx9vKqETWU1ya78c2Yp5pW7syU7z8hUCyhPOUAnpS4GA2JuXTpYzVeMyZ8TNTd7YVvhfLOIMbUbIepPntjX4C85MYbWuQO79uEaCxmtS9ehOcmknO5R8thOL6PlFnCL7yCZGmlSHF7iT9wvEqNXpv9DN1qIIKoksOo8Wfolo0x8MXZyXcnV8rWr55E4vVkIxORhAVbvEgNGu0JyzQ78OBvO0MV0JO1xatbGRf3Qq6FraZrL9dzw1Ymt1SBndbi1CGBzexlI7TL6qe956Pn6Mq5jxGpgybAGoYKMyWEzGiUQYFEC2gS4lAiB5xT7TnbADrBAd4I119LtZF8lDEGNpzbmTa2BMcbq7mDBd4ELJQpnwq9cG25o1xhjhJM1yfCCEipbVMJrc0intY4AZRXQ7dnNT3AP3vV9pOD24MRESeL7cux", 1024);
                printf("文件标识符%d表示的iotest4已被写入\n",fd1);
                close(fd1);
                break;
            }
            case 5:{
                int fd1=open("iotest5", O_RDWR);
                write(fd1, "9c0DDfIRXlyRsUiu4vx402GbLmFrxOGcAzNtKCW3pXk0yVwiugbH3ieuO9bDx98cgsTEqC5aZMGO3mHNA7o7LfBmLlahpqHWXClEkLpFPpNZfoDWj366XzwdTUzfy8reqHMob6k3CfJHcvE2LD7poDpmCTYjLSDn0BwOrZmwYXnm0i6vyOC2vBaQ9isc60eYLQhDukesXgQL6KQaTu9R9BCSlaDtuTtuiOGJqKvLdV3fugVzV6apLbEr5fgHjuj8UWx8lRxhufIVBfIEo9kxuKgtI0EKDNQM7P70aQVAbCNnF3KpQppnfGaph0o369wRZEzcdxrpo7nC71bx9FmrqQCPp0ot5UJfdXi9WA5bnrslFGHacCdrH8gZ3Touki0IpzjGd9WPdOJWVCvPiR9VdJVvV4f8sieHpIY24kwR8lF2UiPfSHaqtZNQZbBaWlsv6EwOlHRngdBQoQm24zeX3mN6z09wI6cn4j3jcVZ4PKVamX8dQPuETlPR9zpgdjF43JDbuXNDgXarkCmn5JdiL6BaH3XoF2DMoqo1EHwpT91R0tsKndfsB1P7HEic8hXkv27IJTV3ZmXAeEanBlLx9vKqETWU1ya78c2Yp5pW7syU7z8hUCyhPOUAnpS4GA2JuXTpYzVeMyZ8TNTd7YVvhfLOIMbUbIepPntjX4C85MYbWuQO79uEaCxmtS9ehOcmknO5R8thOL6PlFnCL7yCZGmlSHF7iT9wvEqNXpv9DN1qIIKoksOo8Wfolo0x8MXZyXcnV8rWr55E4vVkIxORhAVbvEgNGu0JyzQ78OBvO0MV0JO1xatbGRf3Qq6FraZrL9dzw1Ymt1SBndbi1CGBzexlI7TL6qe956Pn6Mq5jxGpgybAGoYKMyWEzGiUQYFEC2gS4lAiB5xT7TnbADrBAd4I119LtZF8lDEGNpzbmTa2BMcbq7mDBd4ELJQpnwq9cG25o1xhjhJM1yfCCEipbVMJrc0intY4AZRXQ7dnNT3AP3vV9pOD24MRESeL7cux", 1024);
                printf("文件标识符%d表示的iotest5已被写入\n",fd1);
                close(fd1);
                break;
            }
        };
    }
    int etick=uptime();
    printf("总时间为%dms\n",(etick-stick)*100);
    unlink("iotest1");
    unlink("iotest2");
    unlink("iotest3");
    unlink("iotest4");
    unlink("iotest5");
    exit(0);
}