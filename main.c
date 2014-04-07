#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define BYTE 8;


// Please note that no input validations are done in this program. This program assumes that only valid inputs are given.
// For all instances, the program will need input parameters for L1 cache, though it can handle parameters without streambuffer and L2 cache.


// Global declarations
   int block_bits = 0, pref_n = 0, pref_m = 0;
   int mem_traffic = 0, full_addr = 0, addr = 0;
   unsigned sets1 = 0, sets2 = 0;

// Variables storing the command line arguments
   int rep_policy = 0;
   int write_policy = 0;
   char trace_file[20];

typedef struct {
   int tag;
   unsigned dirty;
   unsigned valid;
   unsigned LRU_counter;
   unsigned LFU_counter;
} tag;


typedef struct {
   int tag;
   int index_value;
   int addr;
   unsigned valid;

   //unsigned dirty;
   //unsigned LRU_counter;
   //unsigned LFU_counter;
} streambuff;


typedef struct {
   
   int block_size;
   int size;
   int assoc;
   int index_bits;
   int index_mask;
   int tag_mask;
   int prefetches;
   int L2reads_not_L1_prefetch;
   int L2reads_from_L1_prefetch;
   int L2_rm_from_L1_prefetch;

   short unsigned level;
   int reads;
   int read_hits;
   int read_misses;
   int writes;
   int write_hits;
   int write_misses;
   int WB_counter;
   float miss_rate;
   int LRU_counter;
   int *set_age;
   float miss_penalty;
   float hit_time;
   float tot_access_time;
   float avg_access_time;
   //tag **cache;
} cache;


   tag **tag1, **tag2;
   streambuff **sbuff;
   cache *L1, *L2, *SB;


// Function to create a mask from 'a' bit position to 'b' bit position. 
// http://stackoverflow.com/questions/8011700/how-do-i-extract-specific-n-bits-of-a-32-bit-unsigned-integer-in-c
unsigned createMask(unsigned a, unsigned b) {

   unsigned i, r = 0;
   for (i = a; i < b; i++)
       r |= 1 << i;
   return r;
}


tag** create_cache(unsigned rows, int columns) {
 
   int i = 0;
   tag **a;  
   a = calloc(rows, sizeof(tag*));
   for(i = 0; i < rows; i++)
      a[i] = calloc(columns, sizeof(tag));   
   return a; 
}


//Function definitions
int read(cache*, int, tag**, int, int);
int write(cache*, int, tag**, int);
int evict(int, tag**, int, int);
void output();
void prefetch(int, int, streambuff**, int, int);
int compare(const void *, const void *);



int main(int argc, char **argv) {

// Declare the cache and assign input to its corresponding structure variables
   L1 = calloc(1,sizeof(cache));
   L2 = calloc(1,sizeof(cache));
   SB = calloc(1,sizeof(cache));
   L1->level = 1;
   L2->level = 2;

   L1->block_size	 = atoi(argv[1]);
   L2->block_size	 = atoi(argv[1]);
   L1->size		 = atoi(argv[2]); 
   L1->assoc		 = atoi(argv[3]);
   pref_n 		 = atoi(argv[4]); // Number of stream buffers
   pref_m 		 = atoi(argv[5]); // Number of memory blocks in each stream buffer
   L2->size		 = atoi(argv[6]); 
   L2->assoc		 = atoi(argv[7]);
   strcpy(trace_file,argv[8]);

// Local variable declaration starts here
   
   char line[15];
   char a[10];
   char cmd;
   int i=0,z=0;
   unsigned temp;
   FILE *fp;
   
// Calculate number of sets 
   if (L1->assoc != 0)
      sets1 = L1->size / (L1->block_size * L1->assoc);
   else 
      sets1 = 0;

   if (L2->assoc != 0)
      sets2 = L2->size / (L2->block_size * L2->assoc);
   else 
      sets2 = 0;

// Create a tag matrix before reading from trace file

   tag1 = create_cache(sets1,L1->assoc);
   if (L2->size != 0)
      	tag2 = create_cache(sets2,L2->assoc);
   else
 	tag2 = NULL;

   sbuff = calloc(pref_n, sizeof(streambuff*));
   for(i = 0; i < pref_n; i++)
      sbuff[i] = calloc(pref_m, sizeof(streambuff));


   L1->set_age = malloc (sets1 * sizeof(int));
   L2->set_age = malloc (sets2 * sizeof(int));
   SB->set_age = malloc (pref_n * sizeof(int)); // To maintain the stream buffer LRU

// Below code creates mask according to number of sets and block_size for L1 cache
   temp = L1->block_size;
   while (temp >>= 1)
  	block_bits++;

   temp = sets1;
   while (temp >>= 1)
  	L1->index_bits++;

   L1->index_mask = createMask(0,L1->index_bits);
   L1->tag_mask = createMask(L1->index_bits,32 - block_bits);
   L1->tag_mask >>= L1->index_bits;

// Below code creates mask according to number of sets and block_size for L2 cache
   temp = sets2;
   while (temp >>= 1)
  	L2->index_bits++;

   L2->index_mask = createMask(0,L2->index_bits);
   L2->tag_mask = createMask(L2->index_bits,32 - block_bits);
   L2->tag_mask >>= L2->index_bits;

// Read the input trace file and store the command and address
   fp = fopen(trace_file,"r");
   while (fgets(line,14,fp) != NULL) {
  	z++; // For testing purpose
	   if (sscanf(line,"%c %s",&cmd, a) != 0) {
		   full_addr = (int) strtol(a,NULL,16);
  		   addr = full_addr >> block_bits;
	   	   switch (cmd) {
		   
                      case 'r': read(L1,L1->assoc,tag1,addr,0);
				break;

		      case 'w': write(L1,L1->assoc,tag1,addr);
				break;

		   }
	   }
   }

   output();
   return 0;
}


// Do not remove columns parameters even if it is redundant!! 
// For some reason the program is not working when it is removed.
int read(cache *cache, int columns, tag **a, int addr, int prefetch_read) {
  

   if (1==prefetch_read && 2==cache->level)
   	cache->L2reads_from_L1_prefetch++;
    
   cache->LRU_counter++;
   cache->reads += 1;
   int j=0, found=0, sb_found=0, addr2=0, addr3=0, r=0, c=0, d=0, e=0, next=0, evict_row=0;
   int index_value = addr & cache->index_mask;
   int tag_value = (addr >> cache->index_bits) & cache->tag_mask;
   
// Scan all blocks in that row of index_value and update counters if found
   for(j=0;j<cache->assoc;j++) {
	if (a[index_value][j].tag == tag_value) {
		if (a[index_value][j].valid == 1) {  // valid 1 means a non-empty block
		   found = 1;
		   break;
		}
	}
   }

// Simultaneously check for the tag in stream buffer only if the current read call is for L1
// r and c will have the row and col values if there is a hit.
   if (1 == cache->level && pref_n != 0) {
	   for(r=0; r<pref_n; r++) {
		for(c=0; c<pref_m; c++) {
			if (sbuff[r][c].addr == addr && sbuff[r][c].valid == 1) { // check for valid blk also
				//if (sbuff[r][c].index_value == index_value) // this is necessary
					sb_found = 1;
					break;
				
			}	
		}
	   	if (sb_found)
			break;
	   }
   }

// If it is a hit in L1, update read_hits and LRU/LFU counter
   if (found) {

	   cache->read_hits += 1;
	   //cache->LRU_counter++;
	   a[index_value][j].LRU_counter = cache->LRU_counter;
	   a[index_value][j].LFU_counter += 1; // This block is referred. 
	   return 1;
   }


   if (1 == cache->level && pref_n != 0) { // Miss L1, Hit SB

	if (sb_found) {

	cache->read_hits += 1;

        for(j=0;j<cache->assoc;j++) {
              if (a[index_value][j].valid == 0) {
		   found = 1;
 		   break;
	      }
	}

   	if (found) {  // Found empty blocks in L1
	
		a[index_value][j].tag = tag_value;
	 	a[index_value][j].LRU_counter = cache->LRU_counter;
                a[index_value][j].valid = 1;
                // a[index_value][j].dirty = 0; //remember this

	

		// Filling contents of streambuffer
		next = shift_blocks(r,c);

		if (next == 0) {
                	addr3 = addr;
                        prefetch(addr3,pref_m,sbuff,next,r);
                }
                else {
                        //addr3 = sbuff[r][next-1].addr;
                        addr3 = addr + next;
                        prefetch(addr3,pref_m,sbuff,next,r);
                }
		return 1;
   	}

	else {

		j = evict(columns, a, cache->assoc, index_value);

		if (a[index_value][j].dirty == 1) { // write back write allocate

        		cache->WB_counter += 1;
                	a[index_value][j].dirty = 0;
			addr2 = (a[index_value][j].tag << cache->index_bits) | index_value;

			if (1 == cache->level && tag2 != NULL)
				write(L2,L2->assoc,tag2,addr2);
                        else
                                mem_traffic += 1;

			if (1 == cache->level && pref_n != 0) {
	
			   for(d=0; d<pref_n; d++) { // Invalidating streambuffer blocks
                		for(e=0; e<pref_m; e++) {
                	   		if (sbuff[d][e].addr == addr2 && sbuff[d][e].valid == 1) {
                        			sbuff[d][e].valid = 0;
                          		 }
                       		}
                	   }
			}
		}


                a[index_value][j].tag = tag_value;
	        a[index_value][j].LRU_counter = cache->LRU_counter;
                a[index_value][j].valid = 1;
	        //a[index_value][j].dirty = 0;  //remember this

		// Filling contents of streambuffer
                next = shift_blocks(r,c);

		if (next == 0) {
                	addr3 = addr;
                        prefetch(addr3,pref_m,sbuff,next,r);
                }
                else {
                        //addr3 = sbuff[r][next-1].addr;
                        addr3 = addr + next;
                        prefetch(addr3,pref_m,sbuff,next,r);
               }

		return 1;

	}

	}	
	   
   }

// If it is a miss in both cache and stream buffer
  
   cache->read_misses += 1;
   cache->L2reads_not_L1_prefetch++;
   

   if (1==prefetch_read && 2==cache->level)
   	cache->L2_rm_from_L1_prefetch++;

   for(j=0;j<cache->assoc;j++) {
	   if (a[index_value][j].valid == 0) {
		   found = 1;
		   break;
	   }
   }

   if (found) {
	   a[index_value][j].tag = tag_value;
	   a[index_value][j].LRU_counter = cache->LRU_counter;
	   a[index_value][j].valid = 1;

	   if (1 == cache->level) {
	      
	      if (tag2 != NULL)
		   read(L2,L2->assoc,tag2,addr,0);
	      else
		   mem_traffic += 1;

	      if (pref_n != 0) { // if SB is enabled 

		   evict_row = evict_sbuff(SB); // Choose a row and prefetch
		   addr3 = addr;
		   prefetch(addr3,pref_m,sbuff,0,evict_row);
	      }

	   }

	   else
	      mem_traffic += 1;

   return 0;

   }

// No valid block, evict block from cache

   else {

	j = evict(columns, a, cache->assoc, index_value);


        if (a[index_value][j].dirty == 1) { // write back write allocate

	       cache->WB_counter += 1;
	       a[index_value][j].dirty = 0;
	       addr2 = (a[index_value][j].tag << cache->index_bits) | index_value;

	       if (1 == cache->level && tag2 != NULL)
		       write(L2,L2->assoc,tag2,addr2);
	       else
		       mem_traffic += 1;


	       if (1 == cache->level && pref_n != 0) {

	       	  for(d=0; d<pref_n; d++) { // Invalidating streambuffer blocks
		       for(e=0; e<pref_m; e++) {
			       if (sbuff[d][e].addr == addr2 && sbuff[d][e].valid == 1) {
				       sbuff[d][e].valid = 0;
				}
		       }
	          }
	       }	
        }

        if (1 == cache->level) {

	   if (pref_n != 0) { // if SB is enabled 

		   evict_row = evict_sbuff(SB); // Choose a row and prefetch
		   addr3 = addr;
		   prefetch(addr3,pref_m,sbuff,0,evict_row);
	   }

	   if (tag2 != NULL)
		   read(L2,L2->assoc,tag2,addr,0);
	   else
		   mem_traffic += 1;
        }

        else
                mem_traffic += 1;

	a[index_value][j].tag = tag_value;
        a[index_value][j].LRU_counter = cache->LRU_counter;
        a[index_value][j].valid = 1;
	//a[index_value][j].dirty = 0;

   return 0;

   }
}

int write(cache *cache, int columns, tag **a, int addr) {
  
   cache->LRU_counter++;
   cache->writes += 1;
   int j=0, found=0, sb_found=0, addr2=0, addr3=0, r=0, c=0, d=0, e=0, next=0, evict_row=0;
   int index_value = addr & cache->index_mask;
   int tag_value = (addr >> cache->index_bits) & cache->tag_mask;
   
// Scan all blocks in that row of index_value and update counters if found
   for(j=0;j<cache->assoc;j++) {
	if (a[index_value][j].tag == tag_value) {
		if (a[index_value][j].valid == 1) {  // valid 1 means a non-empty block
		   found = 1;
		   break;
		}
	}
   }

// Simultaneously check for the tag in stream buffer only if the current write call is for L1
// r and c will have the row and col values if there is a hit.
   if (1 == cache->level && pref_n != 0) {
	   for(r=0; r<pref_n; r++) {
		for(c=0; c<pref_m; c++) {
			if (sbuff[r][c].addr == addr && sbuff[r][c].valid == 1) { // check for valid blk also
				//if (sbuff[r][c].index_value == index_value) // this is necessary
					sb_found = 1;
					break;
			}	
		}
	   	if (sb_found)
			break;
	   }
   }

// If it is a hit in L1, update read_hits and LRU/LFU counter
   if (found) {

	   cache->write_hits += 1;
	   a[index_value][j].LRU_counter = cache->LRU_counter;
           a[index_value][j].dirty = 1; 
	   return 1;
   }


   if (1 == cache->level && pref_n != 0) { // Miss L1, Hit SB

	if (sb_found) {

	cache->write_hits += 1;

        for(j=0;j<cache->assoc;j++) {
              if (a[index_value][j].valid == 0) {
		   found = 1;
 		   break;
	      }
	}

   	if (found) {  // Found empty blocks in L1
	
		a[index_value][j].tag = tag_value;
	 	a[index_value][j].LRU_counter = cache->LRU_counter;
                a[index_value][j].valid = 1;
                a[index_value][j].dirty = 1; //remember this

	

		// Fetch contents of streambuffer
		next = shift_blocks(r,c);

		if (next == 0) {
                	addr3 = addr;
                        prefetch(addr3,pref_m,sbuff,next,r);
                }
                else {
                        //addr3 = sbuff[r][next-1].addr;
                        addr3 = addr + next;
                        prefetch(addr3,pref_m,sbuff,next,r);
                }
		return 1;
   	}

	else {

		j = evict(columns, a, cache->assoc, index_value);

		if (a[index_value][j].dirty == 1) { // write back write allocate

        		cache->WB_counter += 1;
                	a[index_value][j].dirty = 0;
			addr2 = (a[index_value][j].tag << cache->index_bits) | index_value;

			if (tag2 != NULL)
				write(L2,L2->assoc,tag2,addr2);
                        else
                                mem_traffic += 1;

			//if (1 == cache->level && pref_n != 0) {
	
			   for(d=0; d<pref_n; d++) { // Invalidating streambuffer blocks
                		for(e=0; e<pref_m; e++) {
                	   		if (sbuff[d][e].addr == addr2 && sbuff[d][e].valid == 1) {
                        			sbuff[d][e].valid = 0;
                          		 }
                       		}
                	   }
			//}
		}

		// Filling contents of streambuffer
                next = shift_blocks(r,c);

		if (next == 0) {
                	addr3 = addr;
                        prefetch(addr3,pref_m,sbuff,next,r);
                }
                else {
                        //addr3 = sbuff[r][next-1].addr;
                        addr3 = addr + next;
                        prefetch(addr3,pref_m,sbuff,next,r);
               }

                a[index_value][j].tag = tag_value;
	        a[index_value][j].LRU_counter = cache->LRU_counter;
                a[index_value][j].valid = 1;
	        a[index_value][j].dirty = 1; 

		return 1;
	}
	}	
   }

// If it is a write miss in both cache and stream buffer
 
   cache->write_misses += 1;
   cache->L2reads_not_L1_prefetch++;

   for(j=0;j<cache->assoc;j++) {
	   if (a[index_value][j].valid == 0) {
		   found = 1;
		   break;
	   }
   }

   if (found) {
	   a[index_value][j].tag = tag_value;
	   a[index_value][j].LRU_counter = cache->LRU_counter;
	   a[index_value][j].valid = 1;
	   a[index_value][j].dirty = 1;

	   if (1 == cache->level) {
	      
	      if (pref_n != 0) { // if SB is enabled 

		   evict_row = evict_sbuff(SB); // Choose a row and prefetch
		   addr3 = addr;
		   prefetch(addr3,pref_m,sbuff,0,evict_row);
	      }

	      if (tag2 != NULL)
		   read(L2,L2->assoc,tag2,addr,0);
	      else
		   mem_traffic += 1;
	   }

	   else
	      mem_traffic += 1;

   	   return 0;

   }

// No valid block, evict block from cache

   else {

	j = evict(columns, a, cache->assoc, index_value);


        if (a[index_value][j].dirty == 1) { // write back write allocate

	       cache->WB_counter += 1;
	       a[index_value][j].dirty = 0;
	       addr2 = (a[index_value][j].tag << cache->index_bits) | index_value;

	       if (1 == cache->level && tag2 != NULL)
		       write(L2,L2->assoc,tag2,addr2);
	       else
		       mem_traffic += 1;


	       if (1 == cache->level && pref_n != 0) {

	       	  for(d=0; d<pref_n; d++) { // Invalidating streambuffer blocks
		       for(e=0; e<pref_m; e++) {
			       if (sbuff[d][e].addr == addr2 && sbuff[d][e].valid == 1) {
				       sbuff[d][e].valid = 0;
				}
		       }
	          }
	       }	
        }

	a[index_value][j].tag = tag_value;
        a[index_value][j].LRU_counter = cache->LRU_counter;
        a[index_value][j].valid = 1;
	a[index_value][j].dirty = 1;


        if (1 == cache->level) {

	   if (pref_n != 0) { // if SB is enabled 

		   evict_row = evict_sbuff(SB); // Choose a row and prefetch
		   addr3 = addr;
		   prefetch(addr3,pref_m,sbuff,0,evict_row);
	   }

	   if (tag2 != NULL)
		   read(L2,L2->assoc,tag2,addr,0);
	   else
		   mem_traffic += 1;
        }

        else
                mem_traffic += 1;

   return 0;

   }
}

int shift_blocks(int r, int c) {

   int j = 0, temp = c+1;

   while (temp < pref_m) {
 
      sbuff[r][j].tag = sbuff[r][temp].tag;
      sbuff[r][j].index_value = sbuff[r][temp].index_value;
      sbuff[r][j].addr = sbuff[r][temp].addr;
      sbuff[r][j].valid = sbuff[r][temp].valid;

      j++;
      temp++;
   }

   return j;
}


void prefetch(int a, int m, streambuff **sbuff, int next, int r) {

   // Updating LRU counter
   SB->LRU_counter++;
   SB->set_age[r] = SB->LRU_counter;

   int index_value=0, tag_value=0;
   a = a + 1; // next address = (current_tag + current_index) + 1
   while(next < pref_m) {
      index_value = a & L1->index_mask;
      tag_value = (a >> L1->index_bits) & L1->tag_mask;
      sbuff[r][next].tag = tag_value;
      sbuff[r][next].index_value = index_value;
      sbuff[r][next].addr = a;
      sbuff[r][next].valid = 1;
      L1->prefetches++;

      if (tag2 != NULL)
         read(L2,L2->assoc,tag2,a,1);
      else
         mem_traffic += 1;

      a = a + 1;
      next++;
   }

}

//Based on the replacement policy return the appropriate block for the index_value
int evict(int columns, tag **a, int assoc, int index_value ) {
   unsigned j=0, selected_block=0, temp=0;
   selected_block = j;
   //if (rep_policy == 0) {

      temp = a[index_value][j].LRU_counter;

      // To find the block with minimum LRU_counter
      for(j=0;j<assoc;j++) {
	   if (a[index_value][j].LRU_counter < temp) {
		   temp = a[index_value][j].LRU_counter;
		   selected_block = j;
	   }
      }
   return selected_block;
}


int evict_sbuff(cache *buffer) {

   int j=pref_n-1;
   int selected_block=0, temp=0;
   selected_block = j;
   temp = SB->set_age[j];
  
   for(j; j>=0; j--) {
   	if (SB->set_age[j] < temp) {
                   selected_block = j;
                   temp = SB->set_age[j];
        }
   }

   return selected_block;
}


void output() {

   int i=0,j=0, temp=0, selected_row=0;
   streambuff **tempbuff;
    
// Calculating miss_rate and memory traffic
   L1->miss_rate = (float)(L1->read_misses + L1->write_misses) / (L1->reads + L1->writes);

 
   if (tag2 != NULL) 
      L2->miss_rate = (float)(L2->read_misses - L2->L2_rm_from_L1_prefetch)/L1->L2reads_not_L1_prefetch;

   L1->miss_penalty = 20 + 0.5 * ((float)L1->block_size)/16;
   L1->hit_time = 0.25 + 2.5 * ((float)L1->size/(512 * 1024)) + 0.025 * ((float)L1->block_size/16) + 0.025 * L1->assoc;

   L2->miss_penalty = 20 + 0.5 * ((float)L2->block_size)/16;
   L2->hit_time = 2.5 + 2.5 * ((float)L2->size/(512 * 1024)) + 0.025 * ((float)L2->block_size/16) + 0.025 * L2->assoc;



   L1->tot_access_time = (L1->reads + L1->writes) * L1->hit_time + (L1->read_misses + L1->write_misses) * L1->miss_penalty;

   if (tag1 != NULL)
      L1->avg_access_time = L1->tot_access_time/(L1->reads + L1->writes);
   if (tag2 != NULL)
      L2->avg_access_time = L1->hit_time + (L1->miss_rate * (L2->hit_time + (L2->miss_rate * L2->miss_penalty)));	
// We have all the required values now, printing out the output.

   printf("===== Simulator Configuration =====");
   printf("\nBLOCKSIZE:\t\t\t%d", L1->block_size);
   printf("\nL1_SIZE:\t\t     %d", L1->size);
   printf("\nL1_ASSOC:\t\t\t%d", L1->assoc);
   printf("\nPREF_N:\t\t\t%d", pref_n);
   printf("\nPREF_M:\t\t\t%d", pref_m);
   printf("\nL2_SIZE:\t\t     %d", L2->size);
   printf("\nL2_ASSOC:\t\t\t%d", L2->assoc);
   printf("\ntrace_file:\t\t%s", trace_file);

   printf("\n===== L1 contents =====");

   for (i=0; i<sets1; i++)
      qsort(tag1[i], L1->assoc, sizeof(tag), compare);

   for (i=0; i<sets1; i++) {
      printf("\nSet %d:\t",i);
      for (j=0; j < L1->assoc; j++) {
	   printf("   %x ",tag1[i][j].tag);
	   if (tag1[i][j].dirty)
		printf("D");
      }
   }

   if (pref_n !=0) {

      printf("\n===== L1-SB contents =====\n");
      int *sb_sorted = (int*)malloc((pref_n+1)* sizeof(int));
      int a=0, b=0, c=0, d=0;	
      for(a=0;a<pref_n;a++)
	   sb_sorted[a+1] = SB->set_age[a];

      quicksort(sb_sorted,1,pref_n);

      int temp=0;

      for(b=0;b<pref_n;b++)
      {
	      for(c=0;c<pref_n;c++)
	      {
	      if(SB->set_age[c] == sb_sorted[b+1])
		      {
		      for(d=0;d<pref_m;d++)
			      printf("   %x ",sbuff[c][d].addr << block_bits);
		      printf("\n");	
		      
		      }
	      }	
      }
   }

   if (L2->assoc != 0)

   for (i=0; i<sets2; i++)
      qsort(tag2[i], L2->assoc, sizeof(tag), compare);


   if (tag2 != NULL) {
      
      if (pref_n != 0)
	printf(" ");
      else
        printf("\n");

      printf("===== L2 contents =====");
      for (i=0; i<sets2; i++) {
	 printf("\nSet %d:\t",i);
	 for (j=0; j < L2->assoc; j++) {
	      printf("   %x ",tag2[i][j].tag);
	      if (tag2[i][j].dirty)
		   printf("D");
	 }
      }
   }
 
   if (tag2 != NULL)
	printf("\n");

   printf("===== Simulation results (raw) =====");
   printf("\n a. number of L1 reads:\t\t%d", L1->reads);
   printf("\n b. number of L1 read misses:\t%d", L1->read_misses);
   printf("\n c. number of L1 writes:\t%d", L1->writes);
   printf("\n d. number of L1 write misses:\t%d", L1->write_misses);
   printf("\n e. L1 miss rate:\t\t%.6f", L1->miss_rate);
   printf("\n f. number of L1 writebacks: %d", L1->WB_counter);
   printf("\n g. number of L1 prefetches: %d", L1->prefetches);

   if (tag2 != NULL) {
      
      mem_traffic = L2->read_misses + L2->write_misses + L2->WB_counter;

      printf("\n h. number of L2 reads that did not originate from L1 prefetches: %d", L1->L2reads_not_L1_prefetch);
      printf("\n i. number of L2 read misses that did not originate from L1 prefetches: %d", L2->read_misses - L2->L2_rm_from_L1_prefetch);
      printf("\n j. number of L2 reads that originated from L1 prefetches: %d", L2->L2reads_from_L1_prefetch);
      printf("\n k. number of L2 read misses that originated from L1 prefetches: %d", L2->L2_rm_from_L1_prefetch);
      printf("\n l. number of L2 writes:\t%d", L2->writes);
      printf("\n m. number of L2 write misses:\t%d", L2->write_misses);
      printf("\n n. L2 miss rate:\t\t%.6f", L2->miss_rate);
      printf("\n o. number of L2 writebacks: %d", L2->WB_counter);
      printf("\n p. Total memory traffic:\t%d", mem_traffic);
      printf("\n q. average access time:\t%.6f \n", L2->avg_access_time);

   }

   else {

      mem_traffic = L1->read_misses + L1->write_misses + L1->WB_counter + L1->prefetches;

      printf("\n h. number of L2 reads that did not originate from L1 prefetches: 0");
      printf("\n i. number of L2 read misses that did not originate from L1 prefetches: 0");
      printf("\n j. number of L2 reads that originated from L1 prefetches: 0");
      printf("\n k. number of L2 read misses that originated from L1 prefetches: 0");
      printf("\n l. number of L2 writes:\t 0");
      printf("\n m. number of L2 write misses:\t 0");
      printf("\n n. L2 miss rate:\t\t0");
      printf("\n o. number of L2 writebacks: 0");
      printf("\n p. Total memory traffic:\t%d", mem_traffic);
      printf("\n q. average access time:\t%.6f \n", L1->avg_access_time);

   }
}


int compare(const void *s1, const void *s2) {
 
      tag *t1 = (tag*)s1;
      tag *t2 = (tag*)s2;
      return t2->LRU_counter - t1->LRU_counter;
}


quicksort(int *arr, int p, int r) {
        int q = 0;
        if (p<r) {
                q = partition(arr,p,r);
                quicksort(arr,p,q-1);
                quicksort(arr,q+1,r);
        }
}

partition(int *arr,int p, int r) {

        int x = arr[r];
        int i = p-1,j,temp;
        for(j=p;j<=r-1;j++) {
                if (arr[j] > x) {
                        i = i+1;
                        temp = arr[i];
                        arr[i] = arr[j];
                        arr[j] = temp;
                }
        }
        temp = arr[i+1];
        arr[i+1] = arr[r];
        arr[r] = temp;
        return (i+1);

}
