import argparse
import os
import shutil


def validate_path(path):
    if not isinstance(path, str) or not path.strip():
        raise ValueError(f"Invalid path '{path}'")


def f_create(path):
    try:
        validate_path(path)

        directory = os.path.dirname(path)
        if directory:
            os.makedirs(directory, exist_ok=True)

        # Перезаписываем, если существует
        with open(path, 'w', encoding='utf-8'):
            pass

        print(f"[+] File created successfully: {path}")

    except Exception as e:
        print(f"[-] Error creating file '{path}'. {e}")
        raise


def f_delete(path):
    try:
        validate_path(path)

        if not os.path.exists(path):
            raise FileNotFoundError(f"File not found: {path}")

        if os.path.isdir(path):
            raise IsADirectoryError(f"Path '{path}' is a directory")

        os.remove(path)

        print(f"[+] File deleted successfully: {path}")

    except Exception as e:
        print(f"[-] Error deleting file '{path}'. {e}")
        raise


def f_write(path, content):
    try:
        validate_path(path)

        if content is None:
            raise ValueError(f"Content cannot be None for '{path}'")

        directory = os.path.dirname(path)
        if directory:
            os.makedirs(directory, exist_ok=True)

        with open(path, 'w', encoding='utf-8') as file:
            file.write(str(content))

        print(f"[+] Content written successfully to: {path}")

    except Exception as e:
        print(f"[-] Error writing to file '{path}'. {e}")
        raise


def f_read(path):
    try:
        validate_path(path)

        if not os.path.exists(path):
            raise FileNotFoundError(f"File not found: {path}")

        if os.path.isdir(path):
            raise IsADirectoryError(f"Path '{path}' is a directory")

        with open(path, 'r', encoding='utf-8') as file:
            content = file.read()

        print(f"[+] File read successfully: {path}")
        return content

    except Exception as e:
        print(f"[-] Error reading file '{path}'. {e}")
        raise


def f_copy(src, dest):
    try:
        validate_path(src)
        validate_path(dest)

        if not os.path.exists(src):
            raise FileNotFoundError(f"Source file not found: {src}")

        if os.path.isdir(src):
            raise IsADirectoryError(f"Source '{src}' is a directory")

        if os.path.isdir(dest):
            raise IsADirectoryError(f"Destination '{dest}' is a directory")

        dest_dir = os.path.dirname(dest)
        if dest_dir:
            os.makedirs(dest_dir, exist_ok=True)

        shutil.copy(src, dest)

        print(f"[+] File copied successfully from '{src}' to '{dest}'")

    except Exception as e:
        print(f"[-] Error copying file '{src}'. {e}")
        raise


def f_rename(src, dest):
    try:
        validate_path(src)
        validate_path(dest)

        if not os.path.exists(src):
            raise FileNotFoundError(f"Source file not found: {src}")

        if os.path.isdir(src):
            raise IsADirectoryError(f"Source '{src}' is a directory")

        if os.path.isdir(dest):
            raise IsADirectoryError(f"Destination '{dest}' is a directory")

        dest_dir = os.path.dirname(dest)
        if dest_dir:
            os.makedirs(dest_dir, exist_ok=True)

        os.rename(src, dest)

        print(f"[+] File renamed successfully from '{src}' to '{dest}'")

    except Exception as e:
        print(f"[-] Error renaming file '{src}'. {e}")
        raise


def main():
    parser = argparse.ArgumentParser(description="File system CLI")
    subparsers = parser.add_subparsers(dest='command')

    p_create = subparsers.add_parser('f_create')
    p_create.add_argument('path')

    p_delete = subparsers.add_parser('f_delete')
    p_delete.add_argument('path')

    p_write = subparsers.add_parser('f_write')
    p_write.add_argument('path')
    p_write.add_argument('content')

    p_read = subparsers.add_parser('f_read')
    p_read.add_argument('path')


    p_copy = subparsers.add_parser('f_copy')
    p_copy.add_argument('src')
    p_copy.add_argument('dest')

    p_rename = subparsers.add_parser('f_rename')
    p_rename.add_argument('src')
    p_rename.add_argument('dest')

    args = parser.parse_args()

    try:
        if args.command == 'f_create':
            f_create(args.path)

        elif args.command == 'f_delete':
            f_delete(args.path)

        elif args.command == 'f_write':
            f_write(args.path, args.content)

        elif args.command == 'f_read':
            f_read(args.path)

        elif args.command == 'f_copy':
            f_copy(args.src, args.dest)

        elif args.command == 'f_rename':
            f_rename(args.src, args.dest)

    except Exception:
        pass


if __name__ == '__main__':
    main()
