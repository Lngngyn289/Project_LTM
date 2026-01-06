/**
 * Session Manager - Xử lý logout khi đóng tab/window
 * Giải quyết vấn đề phân biệt giữa:
 * 1. Đóng tab (cần logout)
 * 2. Chuyển trang (không logout)
 * 3. Disconnect tạm thời (không logout)
 */

class SessionManager {
  constructor(socket) {
    this.socket = socket;
    this.isNavigatingAway = false;
    this.isLoggingOut = false;
    this.inactivityTimeout = null;
    this.INACTIVITY_TIME = 15 * 60 * 1000; // 15 phút không hoạt động

    this.setupBeforeUnload();
    this.setupActivityListeners();
    this.resetInactivityTimer();
  }

  /**
   * Setup beforeunload handler để detect khi user đóng tab/window
   * hoặc chuyển sang trang khác
   */
  setupBeforeUnload() {
    window.addEventListener("beforeunload", (event) => {
      // Nếu đang logout thủ công qua nút logout, thì bỏ qua
      if (this.isLoggingOut) {
        return;
      }

      // Đánh dấu là đang chuyển trang/đóng tab
      this.isNavigatingAway = true;

      // Gửi logout request tới server
      this.performLogout();

      // Một số browser cần return value để hiển thị confirm dialog
      // event.preventDefault();
      // event.returnValue = '';
    });
  }

  /**
   * Setup listeners để reset inactivity timer khi có hoạt động
   */
  setupActivityListeners() {
    // Đoạn dự phòng: có thể add thêm events như mousemove, keydown nếu cần
    // window.addEventListener('mousemove', () => this.resetInactivityTimer());
    // window.addEventListener('keydown', () => this.resetInactivityTimer());
  }

  /**
   * Reset timer inactivity
   * Nếu user không hoạt động 15 phút, tự động logout
   */
  resetInactivityTimer() {
    if (this.inactivityTimeout) {
      clearTimeout(this.inactivityTimeout);
    }

    this.inactivityTimeout = setTimeout(() => {
      console.log("User inactive for too long. Auto-logging out...");
      this.performLogout();
      // Sau logout, redirect về login page
      setTimeout(() => {
        sessionStorage.clear();
        window.location.href = "login.html";
      }, 500);
    }, this.INACTIVITY_TIME);
  }

  /**
   * Perform logout - gửi request tới server
   */
  performLogout() {
    try {
      const userId = sessionStorage.getItem("user_id");
      if (userId && this.socket && this.socket.readyState === WebSocket.OPEN) {
        const commandChar = String.fromCharCode(0x14); // CMD_LOGOUT = 0x14
        const message = `${commandChar} ${userId} `;
        this.socket.send(message);
        console.log(`Logout message sent for user ${userId}`);
      }
    } catch (error) {
      console.error("Error sending logout message:", error);
    }
  }

  /**
   * Call này khi user click nút "Logout" - để tránh logout 2 lần
   */
  initiateLogout() {
    this.isLoggingOut = true;
    this.performLogout();
  }

  /**
   * Xóa timer khi session kết thúc
   */
  cleanup() {
    if (this.inactivityTimeout) {
      clearTimeout(this.inactivityTimeout);
    }
  }
}
