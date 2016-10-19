# Compiler issues with tile static memory
This code demonstrates a compiler issue with hcc-lc from the ROCm 1.2 distribution.

## Building and running
To compile with hcc-lc: `make clean; make isa; ./tile_static`. This version demonstrates the bug.

To compile with hcc-hsail: `make clean; make hsail; ./tile_static`. This version gives correct output.

## Testing environment
I compiled and ran all code examples on an i7 6700k with an AMD R9 Nano (Fiji) dGPU, with ROCm 1.2.

## Code description
* Code has four workgroups of four work items each
* First workitem of each group atomically reads/increments a counter, and writes the counter value to tile static memory
* All work items read the value from tile static memory, and store it into an output array. Each work item writes to a 
unique location.
* Program prints the stored values, using one line per workgroup.

```C++
  int values[16];
  extent<1> ext(16);
  array<int, 1> dev_values(ext);
  int counter{0};
  extent<1> counter_ext(1);
  array<int,1> dev_counter(counter_ext, &counter);

  parallel_for_each(ext.tile(4), [=, &dev_values, &dev_counter](tiled_index<1> idx) [[hc]] {
      tile_static int ts;
      
      if(not idx.local[0]){ // only first workitem sets tile static variable
        ts = atomic_fetch_add(&dev_counter[0], 1);
      }
      // everyone reads the tile static variable that has just been set by the first workitem
      // and stores it into a global array
      idx.barrier.wait();
      int val = ts;
      dev_values[idx.global] = val;
    }).wait();

  copy(dev_values, values);

  for(int i = 0; i != 4; ++i){
    for(int j = 0; j != 4; ++j){
      printf("%d  ", values[4*i+j]);
    }
    printf("\n");
  }
```

## Expected output
Program should print four rows of four integers; the four integers within a single row should be identical. 
The values in subsequent rows should be the integers 0, 1, 2, and 3, but the rows may appear in any order.

Expected output example:
```
1 1 1 1
3 3 3 3
0 0 0 0
2 2 2 2
```

## Actual output
When running code compiled with hcc-hsail, the actual output is correct. When running code compiled with hcc-lc, The 2nd to 4th
values in a row are typically different from the first value in the same row. For example,

```
0 3 3 3
1 0 0 0
2 2 2 2
3 1 1 1
```
## Generated assembly
I dumped the assembly for both the version compiled with hcc-lc and hcc-hsail using rocm-gdb, and included them in the repository. 
In the hcc-lc version, all work items *first* do a `ds_read_b32`; then, the execution mask is set so that only the first
work item does an atomic fetch and add, and writes it with a `ds_write_b32`. In other words: the order of the `ds_read_b32`
and `ds_write_b32` is incorrect.

## Variations
The bug does not depend on the use of atomic operations. The following kernel code gives incorrect output with hcc-lc too:

```C++
tile_static int ts;
ts = 666;           // All work items write to the same tile static variable. Admittedly silly,
idx.barrier.wait(); // but legal. Removing these two makes the bug disappear.

if(not idx.local[0]){
  ts = 42;
}
idx.barrier.wait();
int val = ts;
dev_values[idx.global] = val;
```

When compiled with hcc-lc, this gives the following incorrect output:
```
42  666  666  666  
42  666  666  666  
42  666  666  666  
42  666  666  666  
```

When compiled with hcc-hsail, this gives correct output:
```
42  42  42  42  
42  42  42  42  
42  42  42  42  
42  42  42  42  
```

