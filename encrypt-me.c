#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define PAGE_SIZE 4096 // at least for MacOS, can also evaluate everytime using '(sysconf(_SC_PAGESIZE))'

/*
 * Symmetrically encrypt/decrypt input to output.
 * Default is standard input -> standard output.
 * ./encrypt-me
 * [--input=input_file]
 * [--output=output_file]
 * [--decrypt, -d]
*/

/*
 * Arteen's Cipher (Genius)
 * Goal (unlikely): someone who doesn't know it can't figure it out
 * Other goal (more likely): reversible (hopefully)
 *
 * encrypt (after careful consideration):
 *   first pass
 *   ----------
 *   num = number of low-order bits set in the text
 *
 *   XOR = num % UCHAR_MAX
 *   MASK = XOR & ~0x01 (clear the low-order bit for the mask)
 *
 *   second pass
 *   ----------
 *   for each byte (i = 0 -> n-1):
 *     encrpyted byte = (byte ^ MASK) + i
 *
 * decrypt:
 *   for each byte (i = 0 -> n-1):
 *    encrypted byte = encrypted byte - i (yields byte ^ MASK)
 *    num += low order bit set?
 *   XOR = num % CHAR_BIT
 *   MASK = XOR & ~0x01
 *   for each byte:
 *     byte = (encrypted byte) ^ MASK (yields byte)
 *     
 */

_Noreturn void
error(const int status, const char * const str, const char * const prog)
{
  fprintf(stderr, str, prog);
  exit(status);
}

ssize_t
read_into_buf(char * * const buf, const char * const prog)
{
  /*
   * read into buffer up to limit
   * reallocate if necesary an extra "limit" amount
   * read up to that limit
   * repeat and keep track of bytes read, return that
   */
  
  *buf = malloc(PAGE_SIZE);
  if (*buf == NULL)
	{
	  perror("malloc");
	  exit(errno);
	}
  
  ssize_t total_bytes_read = 0;  
  size_t cur_off = 0;
  size_t cur_extra_bytes_read = 0;
  ssize_t bytes_just_read;

  while (
		 (bytes_just_read = read(STDIN_FILENO,
								 *buf + (cur_off * PAGE_SIZE) + cur_extra_bytes_read,
								 PAGE_SIZE - cur_extra_bytes_read)
		  )
		 != 0
		 )
	{
	  
	  if (bytes_just_read == -1)
		{
		  perror("read");
		  exit(errno);
		}

	  if (total_bytes_read > SSIZE_MAX - bytes_just_read)
		error(ERANGE, "%s: integer overflow on read\n", prog);
		  
	  total_bytes_read += bytes_just_read;
	  cur_extra_bytes_read += bytes_just_read;

	  if (cur_extra_bytes_read == PAGE_SIZE)
		{
		  *buf = realloc(*buf, (++cur_off + 1) * PAGE_SIZE);
		  if (*buf == NULL)
			{
			  perror("realloc");
			  exit(errno);
			}
		  cur_extra_bytes_read = 0;
		}

	}

  return total_bytes_read;
}

void write_out_buf(const char * buf, const ssize_t bytes_to_write)
{
  ssize_t bytes_just_written = 0;
  ssize_t bytes_written = 0;

  do
	{
	  bytes_just_written = write(STDOUT_FILENO, buf, bytes_to_write - bytes_written);
	  if (bytes_just_written == -1)
		{
		  perror("write");
		  exit(errno);
		}
	  bytes_written += bytes_just_written;
	}
  while (bytes_written != bytes_to_write);
  
}

void encrypt_me(char * buf, const ssize_t size)
{
  ssize_t low_order_bits_set = 0;

  for (ssize_t i = 0; i < size; i++)
	low_order_bits_set += 0x01 & buf[i];

  unsigned char mask = (low_order_bits_set % UCHAR_MAX) & ~0x01;

  for(ssize_t i = 0; i < size; i++)
	buf[i] = (buf[i] ^ mask) + i;
}

void decrypt_me(char * buf, const ssize_t size)
{
  ssize_t low_order_bits_set = 0;
  
  for (ssize_t i = 0; i < size; i++)
	{
	  buf[i] = buf[i] - i;
	  low_order_bits_set += 0x01 & buf[i];
	}

  unsigned char mask = (low_order_bits_set % UCHAR_MAX) & ~0x01;

  for (ssize_t i = 0; i < size; i++)
	buf[i] = buf[i] ^ mask;
}
  
	  
int
main(int argc, char *argv[])
{
  bool decrypt = false;
  char * input_file = NULL;
  char * output_file = NULL;
  int optc;

  static struct option long_options[] =
	{
	  { "input", required_argument, NULL, 'i' },
	  { "output", required_argument, NULL, 'o' },
	  { "decrypt", no_argument, NULL, 'd' },
	  { NULL, 0, NULL, 0 }
	};

  while (
		 (optc = getopt_long(argc, argv, "-:i:o:d", long_options, NULL))
		 != -1
		 )
	switch (optc)
	  {
	  case 'i':
		if (input_file)
		  error(EXIT_FAILURE, "%s: multiple input options specified\n", argv[0]);
		input_file = optarg;
		break;
	  case 'o':
		if(output_file)
		  error(EXIT_FAILURE, "%s: multiple output options specified\n", argv[0]);
		output_file = optarg;
		break;
	  case 'd':
		if (decrypt)
		  error(EXIT_FAILURE, "%s: why are you specifying multiple '-d' options?\n", argv[0]);
		decrypt = true;
		break;
	  case ':':
		  error(EXIT_FAILURE, "%s: missing option argument\n", argv[0]);
	  case '?':
		  error(EXIT_FAILURE, "%s: unknown option specified\n", argv[0]);
	  case 1:
		  error(EXIT_FAILURE, "%s: non-option argument not allowed\n", argv[0]);		
	  default:
		  error(EXIT_FAILURE, "%s: mysterious parsing error occurred\n", argv[0]);
	  }

  if (input_file)
	{
	  int fd = open(input_file, O_RDONLY);
	  if (fd == -1)
		{
		  perror("open");
		  exit(errno);
		}
	  if (
		  dup2(fd, STDIN_FILENO) == -1
		  )
		{
		  perror("dup2");
		  exit(errno);
		}
	  if (
		  close(fd) == -1
		  )
		{
		  perror("close");
		  exit(errno);
		}
	}

  char * buf;
  ssize_t bytes_read = read_into_buf(&buf, argv[0]);

  if (!decrypt)
	encrypt_me(buf, bytes_read);
  else
	decrypt_me(buf, bytes_read);

  if (output_file)
	{
	  int fd = open(output_file, O_WRONLY|O_CREAT|O_TRUNC,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	  if (-1 == fd)
		{
		  perror("open");
		  exit(errno);
		}
	  if (
		  dup2(fd, STDOUT_FILENO) == -1
		  )
		{
		  perror("dup2");
		  exit(errno);
		}
	  if (
		  close(fd) == -1
		  )
		{
		  perror("close");
		  exit(errno);
		}
	}
  
  write_out_buf(buf, bytes_read);

  return 0;
}
