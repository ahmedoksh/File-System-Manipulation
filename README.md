# File System Manipulation

This project is a command-line utility that interacts with .img filesystem file. It provides four main functionalities:

- `diskinfo`: This command provides meta data of the filesystem.

- `disklist`: This command lists all the files and directories in a given directory in the filesystem.

- `diskget`: This command retrieves a file from the filesystem and places it in your working directory or at a given directory/filename.

- `diskput`: This command takes a file from your current working directory and places it into the filesystem root or to a given directory.

## Getting Started

### Prerequisites
- GCC compiler
- Make

### Building the Project
To build the project using the Makefile, run
```
make
```

### Running the Project
You can run each function as follows:

- `./diskinfo <img-file>`
- `./disklist <img-file> [<dest_dir>]`
- `./diskget <img-file> <file_path> [<dest_dir>]`
- `./diskput <img-file> <file_path> [<dest_dir>]`

Replace `<img-file>` with the path to your .img filesystem image and `<file-name>` with the name of the file you want to retrieve or place into the filesystem. If `<dest_dir>` is not provided, img-file's home folder is used for disklist and diskput, where current directory and filename is used for diskput.
