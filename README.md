# cerver

Simple HTTP server made in C

- Handles routing for `.html`or `.htm` files.
- `/` finds `index.html` or `index.htm`.
- Throws 404 error when file not found.

## Setup

1. Clone the repository and `cd` into it.
2. Run the following command:
   ```shell
   > gcc cerver.c -o cerver
   ```
   The above command will create an executable for cerver.
3. Finally to run the server for folder `path/to/folder`, at the port `XYZ`:
   ```shell
   > ./cerver "path/to/folder" XYZ
   ```
