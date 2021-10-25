 ### Design
Vulkan
  Window Surface (SDL_Vulkan_Create..)
    Swapchain
      <RenderFrame> *async*
        <Shader,Mesh> -> <PipelineBinding>
          
          Pipeline creation categories
            Shader - Shader modules.
            InputAssembly - tris, lines..
            VertexInfo - Vertex.
            One Pipeline per shader, per vertex, per input

          *If no pipeline exists, create pipeline*

          1 CommandBuffer per RenderFrame

            time_step = 0.01;
            Semaphore frameReady

            This isn't valid.
              thread 0
                game->update(fixed_timestep)
              thread 1
                game->update(fixed_timestep)
              thread 2
                game->update(fixed_timestep)


            thread 0
              double t_diff= clock.now() - last
              
              //Fixed timestep updates.
              while(t_diff > 0) {
                //Do not update any graphics here.
                game->updatePhysicsAndGameLogic(fixed_timestep)
                t_diff -= fixed_timestep;
              }
              remainder = abs(t_diff)

              if(frameReady.get()) {
                thread1->
              }
              checkForFrame();
              thread 1 -> draw();

            thread 1
              while(1) {
                bool b = swapchain->frameAvailable()
                if(b) {
                  frameReady.set(thread_0)
                }

              }
            
            beginFrame()
              wait for new image

              game->draw()
                scene->gatherVisibleObjects()
                scene->drawShadows()
                  for all objects
                    frame->commandBuffer->drawObject()
                      pipeline = getPipeline(Mesh, Shader, Material, triangle_list)
                            pipeline = find_pipeline
                            if(pipeline == null)
                              createPipeline()
                                vkQueueWaitIdle
                                ..pipe = new pipeline()..
                              return pipe

                      vkCmdBindPipeline(pipeline)
                      vkCmdBindDescriptorSets
                      vkCmdDrawIndexed()
                scene->drawMeshes()
                scene->drawForward()
              
            endFrame()
              submitQueue
          Framebuffer
          1. Multiple command buffers, or single command buffer. Create it, then submit
          2. Single command buffer is the correct approach.

          Pipeline

    https://stackoverflow.com/questions/53307042/vulkan-when-should-i-create-a-new-pipeline