#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_GPUREADDELEGATE_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_GPUREADDELEGATE_H_

// Permission for a draw-preparation helper thread to run the GPU thread's silent clean-backing
// probe (LibKernel::Memory::TryReadGpuCleanBacking).
//
// That probe reads GPU-thread-owned dirty state (the buffer cache's GPU-modified ranges) without
// a lock, so it is normally refused off the GPU thread. A helper may use it only inside a fork
// window: the GPU thread has handed it a job and, until it joins, performs nothing but the same
// read-only probes (no fallback guest reads, faults, readbacks or cache mutations). The dirty
// state is therefore stable for the whole window. Other threads' mutations of that state already
// synchronize with the GPU thread's probes and so equally with the helper's.
namespace Libs::Graphics::GpuReadDelegate {

inline thread_local bool t_active = false;

[[nodiscard]] inline bool Active() noexcept {
	return t_active;
}

class Scope {
public:
	Scope() noexcept { t_active = true; }
	~Scope() { t_active = false; }
	Scope(const Scope&)            = delete;
	Scope& operator=(const Scope&) = delete;
};

} // namespace Libs::Graphics::GpuReadDelegate

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_GPUREADDELEGATE_H_
