#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define VIRGIN 1

const size_t howbig = 1024;

char* generate_ono(char* customer_id, char* cluster_id, char* ratio)
{
  char* s = malloc(howbig+1);
  snprintf(s, 
	   howbig+1, 
	   "d%d:%sd%d:%s%d:%see", 
	   strlen(customer_id), 
	   customer_id,
	   strlen(cluster_id),
	   cluster_id,
	   strlen(ratio),
	   ratio);
  return s;
}

char* convert_to_length_12(char* n) /* assume n is of length <= 12 */
{
  char* n2 = malloc(13);
  size_t howmany = 12 - strlen(n);
  unsigned int i;
  for (i = 0; i < howmany; i++) n2[i] = '0';
  for (; i < 12; i++) n2[i] = n[i-howmany];
  n2[12] = '\0';
  return n2;
}

char* generate_ip(char* peer_id)
{
  char* ip = malloc(16);
  char* n2 = convert_to_length_12(peer_id);
  ip[0] = n2[0];
  ip[1] = n2[1];
  ip[2] = n2[2];
  ip[3] = '.';
  ip[4] = n2[3];
  ip[5] = n2[4];
  ip[6] = n2[5];
  ip[7] = '.';
  ip[8] = n2[6];
  ip[9] = n2[7];
  ip[10] = n2[8];
  ip[11] = '.';
  ip[12] = n2[9];
  ip[13] = n2[10];
  ip[14] = n2[11];
  ip[15] = '\0';
  free(n2);
  return ip;
}

char* generate_http_request(char* peer_id, char* customer_id, char* cluster_id, char* ratio)
{
  char* hr = malloc(howbig+1);
  char* ip = generate_ip(peer_id);
#ifdef VIRGIN
  snprintf(hr, 
	   howbig+1, 
	   "GET /announce?info_hash=%s&ip=%s&port=%s HTTP/1.0\n",
	   "%4F%152345678901234567%DC%A2",
	   ip,
	   "6881");
#else //apparently opentracker doesn't ignore parameters it doesn't understand
  //the virgin opentracker won't like the ono parameter
  char* ono = generate_ono(customer_id, cluster_id, ratio);
  snprintf(hr, 
	   howbig+1, 
	   "GET /announce?info_hash=%s&ip=%s&port=%s&ono=%s HTTP/1.0\n",
	   "%4F%152345678901234567%DC%A2",
	   ip,
	   "6881",
	   ono);
  free(ono);
#endif
  free(ip);
  return hr;  
}

int main(int argc, char** argv)
{
  if (argc < 3) return -1;
  int howmanytimes = atoi(argv[1]);
  size_t nbytes = 0;
  char peer_id[howbig];
  char customer_id[howbig];
  char cluster_id[howbig];
  char ratio[howbig];
  char* s = malloc(howbig+1);
  char* command = malloc(howbig+1);
  int count = 0;
  while(count < howmanytimes)
  {
    getline(&s, &nbytes, stdin);
    sscanf(s, "%s\t%s\t%s\t%s\n", peer_id, customer_id, cluster_id, ratio);
    //printf("%s\t%s\t%s\t%s\n", peer_id, customer_id, cluster_id, ratio);
    char* http_request = generate_http_request(peer_id, customer_id, cluster_id, ratio);
    //printf("%s\n", http_request);
    snprintf(command,
	     howbig+1,
	     "echo \"%s\" | nc 127.0.0.1 6969",
	     http_request);
    //printf("%s\n", command);
    if (count > 1 && *argv[2] == '1') system(command);
    free(http_request);
    
    count++;
  }
  free(s);
  free(command);
  return 0;
}
