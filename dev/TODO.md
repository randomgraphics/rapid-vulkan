# P0
- Push constant support in pipeline and argument pack
- Sample:
  - rotating triangle with uniform value updated once every frame.
= Pipeline and layout cache

# Resource/Descriptor Design Choices

## **Q**: How to manage multiple set of arguments for multiple draw calls (assuming for single pipeline)
  
  ### usage #1: multiple (mostly) immutable argument packs
  create multiple instance of ArgumentPack instance, one for each draw call. Each one is bind to different resources.
  - pros: minimal render time overhead
  - cons: not intuitive to use.
  ```
  // load/creation time
  auto a1 = createArgumentPack(...);
  a1->i(..);
  a1->b(..);
  a1->c(..);

  auto a2 = createArgumentPack(...);
  a2->i(..);
  a2->b(..);
  a2->c(..);

  // render time
  pipeline->bind(a1);
  pineline->draw()
  pipeline->bind(a2);
  pineline->draw()
  ```

  In this case, holding a pool in each arument pack instance is not effecient. So we need another class to hold descriptor pool.


  ### usage #2: single mutable argument pack
  Only need to create one argument pack instance. The values can change as often as needed.
  - pros: more intuitive to use
  - cons: more overhead in bind() method
  ```
  auto a = createArgumentPack(...);

  a->i(..);
  a->b(..);
  a->c(..);
  pipeline->bind(a);
  pineline->draw()

  a->i(..);
  a->b(..);
  a->c(..);
  pipeline->bind(a); // must call bind() again to apply the change to the   pineline->draw()
  ```

  In this case, should arugment pack class be integrated with Pipeline class?

## Q: What about sharing arguments across multiple pipeline object?

If the ArgumentPack instance is created out of pipeline or pipeline reflection, it won't be easy to share it across mulitple pipeline objects.

**Use case**: a uber ArgumentPack bound to multiple different pipeline objects. Each pipeline only references part of the argument pack.

To support this case, the ArgumentPack class should just be a collection of named arguments. The pipeline object manages all the rest logic.


## Design Choice
- Pipeline objects holds the descriptor set pool
- PipelineLayout object holds layout and setlayout.
- ArgumentPack is just a simple collection of named arguments.
- Each argument is a container of buffer/image/constant
