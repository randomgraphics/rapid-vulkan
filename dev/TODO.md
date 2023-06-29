# P0
- A 3D model viewer that as a complex-enough use case of the library.-
- BUG: simple triangle sample crash when minimized.
- BUG: drawable sample seems leaking small amount of memory every frame.

# P1
- A more powerful render pass class that combined vk::RenderPass and vk::Framebuffers together.
  - this is not a P0 task, since Swapchain's built-in render pass can handle the most common usages already.
- Pipeline and layout cache

# Resource/Descriptor Design Choices

## **Q**: How to manage multiple set of arguments for multiple draw calls (assuming for single pipeline)
  
  ### usage #1: multiple (mostly) immutable argument packs
  create multiple instance of ArgumentPack instance, one for each draw call. Each one is bind to different resources.
  - pros: minimal render time overhead. mimic's the "drwable" concept.
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

  In this case, resource pool is held by the pipeline object.

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
- PipelineLayout object holds layout, set layout and descriptor pool.
- ArgumentPack is just a simple collection of named arguments.
- Each argument is a container of buffer/image/constant
