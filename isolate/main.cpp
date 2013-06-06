#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>



struct context
{
    int argc;
    char **argv;
};

static int child(void *arg)
{
    context &ctx = *(context*)(arg);

    std::cerr << "[RVR] [RUN] decontaminating test chamber\n";

    //uagh clonens + private ns concept seem to be conflicting or something
    //for now we're just going to assume no one wants this shit
    mount(0, "/", 0, MS_REC | MS_PRIVATE, 0);


    // create my own temp universe
    umount ("/tmp");
    mount  (0, "/tmp", "tmpfs", 0, 0);

    mkdir  ("/tmp/root" , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir  ("/tmp/home" , S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);



    std::vector<std::string> mountpoints({
        "/",
        "/data"
    });


    for (std::vector<std::string>::iterator i = mountpoints.begin(); i != mountpoints.end(); i++) {
        mount(i->c_str(), ("/tmp/root" + *i).c_str(), 0,  MS_BIND, 0);
        mount(0,  ("/tmp/root" + *i).c_str(), 0,  MS_REMOUNT | MS_RDONLY | MS_BIND, 0);
    }

    // do we need sys? its polluted with systemd shit
    std::vector<std::string> rwmountpoints({
        "/dev",
        "/run"
    });

    for (std::vector<std::string>::iterator i = mountpoints.begin(); i != mountpoints.end(); i++) {
        mount(i->c_str(), ("/tmp/root" + *i).c_str(), 0,  MS_BIND | MS_REC, 0);
    }

    // new proc
    mount  ("none", "/tmp/root/proc", "proc", 0, 0);


    // rw mount workdir
    char cwd[40000];
    getcwd(cwd, 40000);
    mount (cwd, (std::string("/tmp/root") + cwd).c_str(), 0, MS_BIND,0);

    // chroot
    if (chroot("/tmp/root") != 0)
        throw std::runtime_error("chroot2 failed");

    // dont get stuck in the old cwd with the old mount ns!
    chdir(cwd);


    std::cerr<< "[RVR] [RUN] booting\n";

    execvp(ctx.argv[0], ctx.argv);
    std::cerr<< "[RVR] [RUN] that didn't work :(\n";
    return 3;
}



#define STACK_SIZE (1024 * 1024)    /* Stack size for cloned child */
int main(int argc, char **argv)
{
    if (argc < 2)
        throw std::runtime_error("expected stuff to run as argument");

    std::cerr << "[RVR] [RUN] creating isolation cell\n";

    char *stack;                    /* Start of stack buffer */
    char *stackTop;                 /* End of stack buffer */
    pid_t pid;

    stack = (char*)malloc(STACK_SIZE);
    if (stack == NULL)
        throw std::runtime_error("OOM");

    stackTop = stack + STACK_SIZE;  /* Assume stack grows downward */


    context ctx;
    ctx.argc = argc - 1;
    ctx.argv = argv + 1;


    //TODO without SIGCHLD, waitpid doesnt work. manual says why. later..
    pid = clone(child, stackTop, CLONE_NEWPID | CLONE_NEWNS | SIGCHLD , &ctx);
    if (pid == -1)
        throw std::runtime_error("clone failed");


    int r = 0;
    if (waitpid(pid, &r, 0) == -1)
        throw std::runtime_error("waitpid failed");


    free(stack);


    //should print info about still running children here, just to prove why we need this :/

}
