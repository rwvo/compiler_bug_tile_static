#include <hc.hpp>

using namespace hc;

int main(){
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
      // and stores it into 
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

  return 0;
}
