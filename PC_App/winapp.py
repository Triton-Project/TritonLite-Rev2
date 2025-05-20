import tkinter as tk
from tkinter import messagebox
import customtkinter as ctk
import datetime

# ------------------------------------------------------------
# カラーパレット (CSSの:root変数を参考に)
# ------------------------------------------------------------
COLORS = {
    "primary": "#4285F4",       # Google Blue
    "accent": "#0F9D58",        # Google Green
    "warning": "#FBBC05",       # Google Yellow
    "error": "#EA4335",         # Google Red
    "bg_dark": "#202124",       # Dark background
    "bg_card": "#2D2E31",       # Card background
    "bg_input": "#35363A",      # Input background
    "text_primary": "#E8EAED",  # Primary text
    "text_secondary": "#9AA0A6", # Secondary text
    "border": "#5F6368",        # Border color
    "console_text": "#00FF00",  # Console text (green)
}

# ------------------------------------------------------------
# 修正されたエンコードロジック (React版の仕様に合わせる)
# ------------------------------------------------------------
def calculate_checksum(data_bytes):
    """HEADERから引数で与えられたバイト列の最後までを合計し、下位1Byteをチェックサムとする"""
    return sum(data_bytes) & 0xFF

def encode_data(
    *, year, month, day, hour, minute, second,
    sup_start, sup_stop, exh_start, exh_stop,
    lcd_mode, log_mode, dive_count, press_threshold
):
    header = 0x24  # '$'
    year_offset = year - 2000
    time_bytes = [year_offset, month, day, hour, minute, second]

    sup_start_bytes = sup_start.to_bytes(2, "big")
    sup_stop_bytes = sup_stop.to_bytes(2, "big")
    exh_start_bytes = exh_start.to_bytes(2, "big")
    exh_stop_bytes = exh_stop.to_bytes(2, "big")

    mode_byte = ((lcd_mode & 0x0F) << 4) | (log_mode & 0x0F)
    dive_count_byte = dive_count & 0xFF
    press_threshold_byte = press_threshold & 0xFF

    # チェックサム計算対象データ (React版の順序と範囲に合わせる)
    data_to_checksum = [
        header,
        *time_bytes,
        *sup_start_bytes,
        *sup_stop_bytes,
        *exh_start_bytes,
        *exh_stop_bytes,
        mode_byte, # React版ではmode_byteが先
        dive_count_byte,
        press_threshold_byte,
    ]
    checksum = calculate_checksum(data_to_checksum)
    footer = 0x3B  # ';'

    # 送信フレーム (チェックサムとフッターを含む)
    frame_bytes = data_to_checksum + [checksum, footer]
    return "".join(f"{b:02X}" for b in frame_bytes)

# ------------------------------------------------------------
# CustomTkinter GUI アプリケーション
# ------------------------------------------------------------
class EncoderApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("TRITON-LITE Data Encoder")
        self.geometry("1000x720") # ウィンドウサイズ調整

        ctk.set_appearance_mode("Dark")
        # ctk.set_default_color_theme("blue") # デフォルトのテーマを使用

        self.configure(fg_color=COLORS["bg_dark"])

        self.is_connected = False # 接続状態
        self.entries = {}
        self.current_encoded_data_var = tk.StringVar(value="エンコードデータがここに表示されます")

        self._create_widgets()
        self.update_encoded_data_display() # 初期表示
        self.after(1000, self._update_datetime_and_encoded_data_periodically) # 1秒ごとに日時更新

    def _create_widgets(self):
        # --- ヘッダー ---
        header_frame = ctk.CTkFrame(self, fg_color=COLORS["bg_card"], corner_radius=12, height=70)
        header_frame.pack(fill="x", padx=20, pady=(20,0)) # padyのtopを20に

        logo_frame = ctk.CTkFrame(header_frame, fg_color="transparent")
        logo_frame.pack(side="left", padx=20, pady=10)

        logo_text_triton = ctk.CTkLabel(logo_frame, text="TRITON", font=ctk.CTkFont(family="Product Sans", size=30, weight="bold"), text_color=COLORS["primary"])
        logo_text_triton.pack(side="left")
        logo_text_lite = ctk.CTkLabel(logo_frame, text="-LITE", font=ctk.CTkFont(family="Product Sans", size=22, weight="normal"), text_color=COLORS["accent"])
        logo_text_lite.pack(side="left", anchor="s", pady=(0,2)) # 少し下に

        status_frame = ctk.CTkFrame(header_frame, fg_color=COLORS["bg_input"], corner_radius=24)
        status_frame.pack(side="right", padx=20, pady=10)
        
        self.status_dot = ctk.CTkFrame(status_frame, width=10, height=10, corner_radius=5, fg_color=COLORS["error"])
        self.status_dot.pack(side="left", padx=(10,5))
        self.status_label = ctk.CTkLabel(status_frame, text="Disconnected", text_color=COLORS["text_primary"], font=ctk.CTkFont(family="Roboto", size=14))
        self.status_label.pack(side="left", padx=(0,10), pady=5)

        # --- メインコンテンツ ---
        main_content_frame = ctk.CTkFrame(self, fg_color="transparent")
        main_content_frame.pack(fill="both", expand=True, padx=20, pady=20)
        main_content_frame.grid_columnconfigure(0, weight=1) # 左パネル
        main_content_frame.grid_columnconfigure(1, weight=1) # 右パネル
        main_content_frame.grid_rowconfigure(0, weight=1)

        # --- 左パネル ---
        left_panel = ctk.CTkFrame(main_content_frame, fg_color="transparent")
        left_panel.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        
        self._create_connection_card(left_panel)
        self._create_parameters_card(left_panel)

        # --- 右パネル ---
        right_panel = ctk.CTkFrame(main_content_frame, fg_color="transparent")
        right_panel.grid(row=0, column=1, sticky="nsew", padx=(10, 0))
        self._create_output_card(right_panel)

    def _create_connection_card(self, parent):
        conn_card = ctk.CTkFrame(parent, fg_color=COLORS["bg_card"], corner_radius=12)
        conn_card.pack(fill="x", pady=(0, 20))

        title = ctk.CTkLabel(conn_card, text="Connection", font=ctk.CTkFont(family="Roboto", size=18, weight="bold"), text_color=COLORS["primary"], anchor="w")
        title.pack(fill="x", padx=24, pady=(15,5))
        title_underline = ctk.CTkFrame(conn_card, height=3, width=40, fg_color=COLORS["primary"], corner_radius=2)
        title_underline.pack(anchor="w", padx=24, pady=(0,10))

        controls_frame = ctk.CTkFrame(conn_card, fg_color="transparent")
        controls_frame.pack(fill="x", padx=24, pady=(0,20))
        controls_frame.grid_columnconfigure((0,1), weight=1)

        self.connect_btn = ctk.CTkButton(
            controls_frame, text="🔌 Connect", command=self.toggle_connection,
            font=ctk.CTkFont(family="Roboto", size=14, weight="bold"),
            fg_color=COLORS["primary"], hover_color="#3367D6", text_color="white",
            corner_radius=24, height=40
        )
        self.connect_btn.grid(row=0, column=0, padx=(0,5), sticky="ew")

        self.disconnect_btn = ctk.CTkButton(
            controls_frame, text="🚫 Disconnect", command=self.toggle_connection,
            font=ctk.CTkFont(family="Roboto", size=14, weight="bold"),
            fg_color=COLORS["error"], hover_color="#D73127", text_color="white",
            corner_radius=24, height=40, state="disabled"
        )
        self.disconnect_btn.grid(row=0, column=1, padx=(5,0), sticky="ew")

    def _create_parameters_card(self, parent):
        params_card = ctk.CTkScrollableFrame(parent, fg_color=COLORS["bg_card"], corner_radius=12) # Scrollable for many params
        params_card.pack(fill="both", expand=True)

        # --- タイミングパラメータ ---
        timing_title = ctk.CTkLabel(params_card, text="Timing Parameters", font=ctk.CTkFont(family="Roboto", size=18, weight="bold"), text_color=COLORS["primary"], anchor="w")
        timing_title.pack(fill="x", padx=24, pady=(15,5))
        timing_title_underline = ctk.CTkFrame(params_card, height=3, width=40, fg_color=COLORS["primary"], corner_radius=2)
        timing_title_underline.pack(anchor="w", padx=24, pady=(0,10))

        timing_grid = ctk.CTkFrame(params_card, fg_color="transparent")
        timing_grid.pack(fill="x", padx=24, pady=(0,15))
        timing_grid.grid_columnconfigure((0,1), weight=1)
        
        self.input_fields_timing = [
            ("sup_start", "Sup Start", 65535, 0, "0", "s"),
            ("sup_stop", "Sup Stop", 65535, 0, "0", "ms"),
            ("exh_start", "Exh Start", 65535, 0, "0", "s"),
            ("exh_stop", "Exh Stop", 65535, 0, "0", "ms"),
        ]
        for i, (key, label, max_v, min_v, def_v, unit) in enumerate(self.input_fields_timing):
            self._create_form_group(timing_grid, key, label, max_v, min_v, def_v, unit, row=i//2, col=i%2)

        # --- モード設定 ---
        mode_title = ctk.CTkLabel(params_card, text="Mode Settings", font=ctk.CTkFont(family="Roboto", size=18, weight="bold"), text_color=COLORS["primary"], anchor="w")
        mode_title.pack(fill="x", padx=24, pady=(15,5))
        mode_title_underline = ctk.CTkFrame(params_card, height=3, width=40, fg_color=COLORS["primary"], corner_radius=2)
        mode_title_underline.pack(anchor="w", padx=24, pady=(0,10))

        mode_grid = ctk.CTkFrame(params_card, fg_color="transparent")
        mode_grid.pack(fill="x", padx=24, pady=(0,15))
        mode_grid.grid_columnconfigure((0,1), weight=1)

        self.input_fields_mode = [
            ("lcd_mode", "LCD Mode", 15, 0, "0", None),
            ("log_mode", "Log Mode", 15, 0, "0", None),
        ]
        for i, (key, label, max_v, min_v, def_v, unit) in enumerate(self.input_fields_mode):
            self._create_form_group(mode_grid, key, label, max_v, min_v, def_v, unit, row=i//2, col=i%2)
            
        # --- ダイビングパラメータ ---
        diving_title = ctk.CTkLabel(params_card, text="Diving Parameters", font=ctk.CTkFont(family="Roboto", size=18, weight="bold"), text_color=COLORS["primary"], anchor="w")
        diving_title.pack(fill="x", padx=24, pady=(15,5))
        diving_title_underline = ctk.CTkFrame(params_card, height=3, width=40, fg_color=COLORS["primary"], corner_radius=2)
        diving_title_underline.pack(anchor="w", padx=24, pady=(0,10))

        diving_grid = ctk.CTkFrame(params_card, fg_color="transparent")
        diving_grid.pack(fill="x", padx=24, pady=(0,15))
        diving_grid.grid_columnconfigure((0,1), weight=1) # 1列にするなら (0), weight=1

        self.input_fields_diving = [
            ("dive_count", "Dive Count (0 for unlimited)", 255, 0, "0", "回"), # React版は1023だがPython版は255
            ("press_threshold", "Pressure Threshold", 255, 0, "0", None), # React版は1023だがPython版は255
        ]
        for i, (key, label, max_v, min_v, def_v, unit) in enumerate(self.input_fields_diving):
             self._create_form_group(diving_grid, key, label, max_v, min_v, def_v, unit, row=i, col=0, colspan=2) # 1列で表示

        # --- エンコードデータ表示 ---
        encoded_display_frame = ctk.CTkFrame(params_card, fg_color="transparent")
        encoded_display_frame.pack(fill="x", padx=24, pady=(10,0))
        
        encoded_label = ctk.CTkLabel(encoded_display_frame, text="Encoded Data (HEX):", font=ctk.CTkFont(family="Roboto", size=14), text_color=COLORS["text_secondary"], anchor="w")
        encoded_label.pack(fill="x")
        
        encoded_entry = ctk.CTkEntry(
            encoded_display_frame, textvariable=self.current_encoded_data_var, state="readonly",
            font=ctk.CTkFont(family="Roboto Mono", size=12), text_color=COLORS["text_primary"],
            fg_color=COLORS["bg_input"], border_color=COLORS["border"], corner_radius=8, height=35
        )
        encoded_entry.pack(fill="x", pady=(5,15), ipady=3)

        # --- 送信ボタン ---
        send_btn = ctk.CTkButton(
            params_card, text="➤ Send Data", command=self.send_data_action,
            font=ctk.CTkFont(family="Roboto", size=16, weight="bold"),
            fg_color=COLORS["accent"], hover_color="#0B8043", text_color="white",
            corner_radius=24, height=45, state="disabled" # Initially disabled
        )
        send_btn.pack(fill="x", padx=24, pady=(5,20), ipady=5)
        self.send_btn = send_btn # アクセス可能にする

    def _create_form_group(self, parent, key, label_text, max_val, min_val, default_val, unit, row, col, colspan=1):
        group = ctk.CTkFrame(parent, fg_color="transparent")
        group.grid(row=row, column=col, columnspan=colspan, sticky="ew", padx=5, pady=8)

        label = ctk.CTkLabel(group, text=label_text, font=ctk.CTkFont(family="Roboto", size=14), text_color=COLORS["text_secondary"], anchor="w")
        label.pack(fill="x")

        input_wrapper = ctk.CTkFrame(group, fg_color="transparent")
        input_wrapper.pack(fill="x", pady=(3,0))

        entry_var = tk.StringVar(value=default_val)
        entry = ctk.CTkEntry(
            input_wrapper, textvariable=entry_var,
            font=ctk.CTkFont(family="Roboto", size=16), text_color=COLORS["text_primary"],
            fg_color=COLORS["bg_input"], border_color=COLORS["border"], corner_radius=8,
            width=100, # Adjust width as needed
            state="disabled" # Initially disabled
        )
        entry.pack(side="left", fill="x", expand=True)
        entry_var.trace_add("write", lambda *args, kv=key: self._handle_parameter_change(kv))


        if unit:
            unit_label = ctk.CTkLabel(input_wrapper, text=unit, font=ctk.CTkFont(family="Roboto", size=14), text_color=COLORS["text_secondary"], width=30, anchor="e")
            unit_label.pack(side="right", padx=(5,0))
            entry.pack_configure(padx=(0,5)) # エントリとユニットの間に少しスペース

        self.entries[key] = (entry_var, min_val, max_val, entry_var.get(), entry) # (var, min, max, last_valid_value, widget)

    def _create_output_card(self, parent):
        output_card = ctk.CTkFrame(parent, fg_color=COLORS["bg_card"], corner_radius=12)
        output_card.pack(fill="both", expand=True)

        output_header = ctk.CTkFrame(output_card, fg_color="transparent", height=50)
        output_header.pack(fill="x", padx=24, pady=(15,0))

        title = ctk.CTkLabel(output_header, text="Console", font=ctk.CTkFont(family="Roboto", size=18, weight="bold"), text_color=COLORS["primary"], anchor="w")
        title.pack(side="left", pady=(0,5))
        # title_underline = ctk.CTkFrame(output_header, height=3, width=40, fg_color=COLORS["primary"], corner_radius=2)
        # title_underline.pack(side="left", anchor="w", padx=(0,0), pady=(0,10)) # Underline for console title?

        clear_btn = ctk.CTkButton(
            output_header, text="Clear", command=self.clear_console,
            font=ctk.CTkFont(family="Roboto", size=14), text_color=COLORS["text_secondary"],
            fg_color="transparent", hover_color=COLORS["bg_input"], border_width=1, border_color=COLORS["border"],
            width=80, height=30, corner_radius=15
        )
        clear_btn.pack(side="right")

        self.console_output = ctk.CTkTextbox(
            output_card, font=ctk.CTkFont(family="Roboto Mono", size=13),
            text_color=COLORS["console_text"], fg_color="#1A1A1C", corner_radius=8,
            border_color=COLORS["border"], border_width=1,
            activate_scrollbars=True, state="disabled" # Read-only
        )
        self.console_output.pack(fill="both", expand=True, padx=24, pady=20)
        self.add_to_console("TRITON-LITE Control Interface ready.")
        if not hasattr(navigator, 'serial') if 'navigator' in globals() else True : # Placeholder for browser check
             self.add_to_console("Serial API (Web Serial) typically used in browsers. This is a desktop app.")


    def _handle_parameter_change(self, param_key):
        # This is called on every character change.
        # We will validate and update the encoded string.
        self.update_encoded_data_display()

    def _update_datetime_and_encoded_data_periodically(self):
        # This updates the time component and re-encodes.
        self.update_encoded_data_display()
        self.after(1000, self._update_datetime_and_encoded_data_periodically)


    def get_validated_params(self):
        params = {}
        all_valid = True
        for key, (var, min_val, max_val, last_valid, widget) in self.entries.items():
            try:
                value_str = var.get()
                if not value_str and min_val == 0: # Allow empty for 0 if min is 0
                    value = 0
                elif not value_str:
                    value = int(last_valid) # revert to last valid if empty and not allowed
                    var.set(str(value))
                else:
                    value = int(value_str)

                if not (min_val <= value <= max_val):
                    # Revert to last valid value or clamp
                    clamped_value = max(min_val, min(value, max_val))
                    # var.set(str(last_valid)) # Option 1: Revert
                    var.set(str(clamped_value)) # Option 2: Clamp
                    value = clamped_value
                    # self.add_to_console(f"Warning: {key} out of range ({min_val}-{max_val}). Value clamped to {value}.", COLORS["warning"])
                
                params[key] = value
                self.entries[key] = (var, min_val, max_val, str(value), widget) # Update last_valid
                widget.configure(border_color=COLORS["border"]) # Reset border
            except ValueError:
                # Invalid integer, revert to last valid value
                var.set(last_valid)
                params[key] = int(last_valid)
                widget.configure(border_color=COLORS["error"]) # Highlight error
                all_valid = False
        
        if not all_valid:
            self.add_to_console("Invalid input detected. Reverted to last valid value(s).", COLORS["warning"])

        # 日時情報
        dt_now = datetime.datetime.now()
        params['year'] = dt_now.year
        params['month'] = dt_now.month
        params['day'] = dt_now.day
        params['hour'] = dt_now.hour
        params['minute'] = dt_now.minute
        params['second'] = dt_now.second
        return params, all_valid


    def update_encoded_data_display(self):
        if not self.entries: # Widgets not created yet
            return
            
        params, is_valid = self.get_validated_params()
        if params: # if get_validated_params didn't return None
            try:
                encoded_string = encode_data(**params)
                self.current_encoded_data_var.set(encoded_string.upper())
            except Exception as e:
                self.current_encoded_data_var.set("Error in encoding!")
                self.add_to_console(f"Encoding Error: {e}", COLORS["error"])

    def toggle_connection(self):
        self.is_connected = not self.is_connected
        if self.is_connected:
            self.status_dot.configure(fg_color=COLORS["accent"])
            self.status_label.configure(text="Connected")
            self.connect_btn.configure(state="disabled", text="🔌 Connected")
            self.disconnect_btn.configure(state="normal")
            self.send_btn.configure(state="normal")
            for _key, (_var, _min, _max, _last_valid, widget) in self.entries.items():
                widget.configure(state="normal")
            self.add_to_console("Device connected (simulated). Parameters enabled.")
        else:
            self.status_dot.configure(fg_color=COLORS["error"])
            self.status_label.configure(text="Disconnected")
            self.connect_btn.configure(state="normal", text="🔌 Connect")
            self.disconnect_btn.configure(state="disabled")
            self.send_btn.configure(state="disabled")
            for _key, (_var, _min, _max, _last_valid, widget) in self.entries.items():
                widget.configure(state="disabled")
            self.add_to_console("Device disconnected (simulated). Parameters disabled.")
        self.update_encoded_data_display() # Update display based on state

    def send_data_action(self):
        if not self.is_connected:
            self.add_to_console("Error: Not connected. Cannot send data.", COLORS["error"])
            return

        encoded_data = self.current_encoded_data_var.get()
        if "Error" in encoded_data or not encoded_data:
            self.add_to_console("Error: Invalid data to send.", COLORS["error"])
            return
            
        self.add_to_console(f"Sending data: {encoded_data}")
        # ここで実際のシリアル送信処理を実装する
        # 例: self.serial_port.write(bytes.fromhex(encoded_data))
        self.add_to_console("Data sent successfully (simulated).")

    def add_to_console(self, message, color=None):
        self.console_output.configure(state="normal") # Enable writing
        timestamp = datetime.datetime.now().strftime("%H:%M:%S")
        
        # For colored messages, we need to use tags
        if color:
            tag_name = f"color_{color.replace('#', '')}" # Create a unique tag name
            self.console_output.tag_config(tag_name, foreground=color)
            self.console_output.insert("end", f"[{timestamp}] ", ("timestamp_tag",))
            self.console_output.insert("end", f"{message}\n", (tag_name,))

        else: # Default color
            self.console_output.insert("end", f"[{timestamp}] {message}\n")

        self.console_output.tag_config("timestamp_tag", foreground=COLORS["text_secondary"])
        self.console_output.see("end") # Scroll to end
        self.console_output.configure(state="disabled") # Disable writing

    def clear_console(self):
        self.console_output.configure(state="normal")
        self.console_output.delete("1.0", "end")
        self.console_output.configure(state="disabled")
        self.add_to_console("Console cleared.")

# ------------------------------------------------------------
# 実行ブロック
# ------------------------------------------------------------
if __name__ == "__main__":
    app = EncoderApp()
    app.mainloop()