///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include <cppcoro/detail/linux.hpp>
#include <system_error>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <sys/utsname.h>

#include <tuple>
#include <charconv>

namespace cppcoro {
    namespace detail {
        namespace lnx {
            uring_queue::uring_queue(size_t queue_length, uint32_t flags) {
                check_required_kernel(5, 4, "initializing uring library");
                auto err = io_uring_queue_init(queue_length, &ring_, flags);
                if (err < 0) {
                    throw std::system_error
                        {
                            static_cast<int>(-err),
                            std::system_category(),
                            "Error initializing uring"
                        };
                }
            }

            uring_queue::~uring_queue() noexcept {
                io_uring_queue_exit(&ring_);
            }

            bool uring_queue::dequeue(void *&msg, message_type &type, bool wait) {
                io_uring_cqe *cqe;
                int ret;
                if (wait)
                    ret = io_uring_wait_cqe(&ring_, &cqe);
                else ret = io_uring_peek_cqe(&ring_, &cqe);
                if (ret == -EAGAIN) {
                    return false;
                } else if (ret < 0) {
                    throw std::system_error{-ret,
                                            std::system_category(),
                                            std::string{"io_uring_peek_cqe failed"}};
                } else {
                    io_uring_cqe_seen(&ring_, cqe);
                    auto res = cqe->res;
                    message *msg_ptr = reinterpret_cast<message *>(io_uring_cqe_get_data(cqe));
                    if (msg_ptr == nullptr) {
                        return true; // cancelled
                    }
                    msg = msg_ptr->m_ptr;
                    type = msg_ptr->m_type;
                    msg_ptr->m_result = res;
                    return true;  // completed
                }
            }

            safe_fd create_event_fd() {
                int fd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK | EFD_CLOEXEC);

                if (fd == -1) {
                    throw std::system_error
                        {
                            static_cast<int>(errno),
                            std::system_category(),
                            "Error creating io_service: event fd create"
                        };
                }

                return safe_fd{fd};
            }

            void safe_fd::close() noexcept {
                if (m_fd != -1) {
                    ::close(m_fd);
                    m_fd = -1;
                }
            }

            static auto get_kernel_version() {
                utsname buffer;
                if (uname(&buffer) != 0) {
                    throw std::system_error(
                        errno,
                        std::generic_category()
                    );
                }
                int major, minor;
                const char *release = &buffer.release[0];
                const char *release_end = release + _UTSNAME_RELEASE_LENGTH;
                release = std::from_chars(release, release_end, major).ptr;
                ++release;
                release = std::from_chars(release, release_end, minor).ptr;
                return std::tuple{major, minor};
            }

            void check_required_kernel(int major, int minor, std::string_view message) {
                static auto[detected_major, detected_minor] = get_kernel_version();
                if (detected_major < major
                    or (detected_major <= major
                        and detected_minor < minor)) {
                    throw std::runtime_error(std::string(message.begin(), message.end()) +
                        ": invalid kernel detected, " + std::to_string(major) + '.' + std::to_string(minor) +
                        " required, " + std::to_string(detected_major) + '.' + std::to_string(detected_minor) + " detected.");
                }
            }
        }
    }
}
