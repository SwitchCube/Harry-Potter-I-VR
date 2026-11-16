"""Local desktop launcher. Normal sessions use the original six-slot menu."""
from mod_locale import tr
import queue
import threading
import tkinter as tk
from tkinter import messagebox
from project_paths import inside
from play_sessions import run, atomic_json
from probe_session import request_stop


class Launcher:
    def __init__(self):
        self.root = tk.Tk(); self.root.title('Harry Potter I — Flat & VR')
        self.root.geometry('660x670'); self.root.resizable(False, False)
        self.root.configure(bg='#101827')
        try: self.root.iconbitmap(str(inside('Harry Potter TM/System/hp.ico')))
        except tk.TclError: pass
        self.events = queue.Queue(); self.running = False
        self.mode = tk.StringVar(value='VR')
        settings = inside('config/launcher.json')
        if settings.exists():
            import json
            saved = json.loads(settings.read_text(encoding='utf-8-sig')).get('mode')
            if saved in ('Flat','VR'): self.mode.set(saved)
        frame = tk.Frame(self.root, bg='#101827'); frame.pack(fill='both', expand=True, padx=32, pady=24)
        self.label(frame, 'HARRY POTTER I', 28, '#ebcc86', 'Georgia').pack(pady=(4, 0))
        self.label(frame, tr('Der Stein der Weisen',"The Philosopher's Stone"), 15, '#d0d8e7').pack(pady=(2, 6))
        self.label(frame, tr('Wähle, wie du spielen möchtest.', 'Choose how you would like to play.'), 11, '#9aadc5').pack(pady=(4, 20))
        choices = tk.Frame(frame, bg='#101827'); choices.pack(fill='x')
        self.radios = []
        for value, title in [('Flat',tr('FLAT\nOriginales Spiel',"FLAT\nOriginal game")), ('VR',tr('VR\nSteamVR & Controller', 'VR\nSteamVR & controllers'))]:
            button = tk.Radiobutton(choices, text=title, value=value, variable=self.mode, indicatoron=False, command=self.selection,
                font=('Segoe UI', 13), bg='#1c2b43', fg='#edf3ff', selectcolor='#35527e', activebackground='#405f8f', activeforeground='white',
                relief='flat', borderwidth=2, highlightthickness=1, highlightbackground='#526a88', padx=10, pady=16, cursor='hand2', takefocus=True)
            button.pack(side='left', fill='x', expand=True, padx=5); self.radios.append(button)
        self.description = tk.StringVar()
        tk.Label(frame, textvariable=self.description, font=('Segoe UI', 11), wraplength=580, height=3, justify='center', bg='#101827', fg='#d1dded').pack(fill='x', pady=(12, 5))
        self.start = tk.Button(frame, text=tr('Spiel starten',"Start game"), command=self.start_game, bg='#d7b56d', fg='#172136', activebackground='#f0d399', font=('Segoe UI', 14, 'bold'), relief='flat', cursor='hand2', pady=9)
        self.start.pack(fill='x', padx=5, pady=8)
        self.label(frame, tr('Im Spielmenü: „Spiel starten“ → freien Speicherplatz wählen.\nZum Fortsetzen denselben belegten Speicherplatz auswählen.', 'In the game menu, select Start Game and an empty save slot.\nTo continue, select the same occupied slot.'), 10, '#b7c7dc').pack(pady=(6, 8))
        self.label(frame, tr('Flat und VR teilen deinen Fortschritt. Keine Zeitbegrenzung.\nGespeichert wird an den Speicherpunkten des Originalspiels.',"Flat and VR share your progress. No time limit.\nThe game saves at its original save points."), 10, '#b7c7dc').pack(pady=(0, 12))
        self.status = tk.StringVar(value=tr('Bereit. Der VR-Mod ist noch in Entwicklung.',"Ready. This VR mod is still in development."))
        tk.Label(frame, textvariable=self.status, font=('Segoe UI', 10), wraplength=580, height=3, justify='center', bg='#101827', fg='#ebcc86').pack(fill='x')
        bottom = tk.Frame(frame, bg='#101827'); bottom.pack(fill='x', pady=(4, 0))
        self.stop = tk.Button(bottom, text=tr('Spiel beenden',"Stop game"), command=self.stop_game, state='disabled', bg='#23354e', fg='white', disabledforeground='#738299', relief='flat', padx=14, pady=6)
        self.stop.pack(side='left')
        tk.Button(bottom, text=tr('Schließen', 'Close'), command=self.close, bg='#23354e', fg='white', relief='flat', padx=14, pady=6).pack(side='right')
        self.root.protocol('WM_DELETE_WINDOW', self.close)
        self.root.bind('<Return>', lambda event: self.start_game() if not self.running else None)
        self.selection(); self.root.after(200, self.poll)

    def label(self, parent, text, size, color, font='Segoe UI'):
        return tk.Label(parent, text=text, font=(font, size), bg='#101827', fg=color, justify='center')

    def selection(self):
        self.description.set(tr('Originale Grafik und Steuerung mit Maus und Tastatur.\nKeine VR-Erweiterungen im Spiel.',"Original graphics with mouse and keyboard controls.\nNo VR modifications in the game.") if self.mode.get() == 'Flat' else tr('Räumliche Ansicht und Controller-Steuerung.\nVor dem Start SteamVR öffnen und das Headset verbinden.', 'Stereo VR with motion controllers.\nOpen SteamVR and connect your headset before starting.'))

    def start_game(self):
        if self.running: return
        mode = self.mode.get(); atomic_json('config/launcher.json', {'mode': mode})
        self.running = True; self.start.configure(state='disabled'); self.stop.configure(state='normal')
        for button in self.radios: button.configure(state='disabled')
        self.status.set(tr('Spiel wird vorbereitet … Das Fenster bleibt während des Spiels geöffnet.', 'Preparing the game… This window stays open while you play.'))
        def work():
            try: self.events.put(('done', run(mode)))
            except Exception as error: self.events.put(('error', str(error)))
        threading.Thread(target=work, daemon=False).start()

    def poll(self):
        try:
            event, result = self.events.get_nowait()
            self.running = False; self.start.configure(state='normal'); self.stop.configure(state='disabled')
            for button in self.radios: button.configure(state='normal')
            self.root.deiconify()
            if event == 'error':
                self.status.set(tr('Start nicht möglich.', 'Unable to start the game.')); messagebox.showerror('Harry Potter I', result, parent=self.root)
            elif result['exit_code']:
                self.status.set(tr('Das Spiel wurde unterbrochen. Bereits geschriebene Spielstände bleiben erhalten.', 'The game was interrupted. Completed saves have been kept.'))
                messagebox.showerror(tr('Spielsitzung unterbrochen',"Game session interrupted"), tr('Der Spielprozess wurde nicht regulär beendet.\nDein Profil bleibt erhalten.\n\nProtokoll: ', 'The game did not exit normally.\nYour profile has been kept.\n\nLog: ')+result['log'], parent=self.root)
            elif result.get('warnings'):
                self.status.set(tr('Sitzung beendet. Die zusätzliche Sicherung konnte nicht vollständig erstellt werden.', 'Session ended. The additional backup could not be completed.'))
                messagebox.showwarning(tr('Sicherung unvollständig', 'Incomplete backup'), tr('Die Sitzung und ihre Spielstände bleiben im Projektordner erhalten.\nDie zusätzliche Sicherung benötigt eine Prüfung.\n\nProfil: ', 'The session and its saves remain in the project folder.\nThe additional backup needs to be checked.\n\nProfile: ')+result['profile']+'\n\n'+'\n'.join(result['warnings']), parent=self.root)
            else:
                self.status.set(tr('Sitzung beendet. Wähle Flat oder VR, um über das Spielmenü fortzusetzen.', 'Session ended. Choose Flat or VR to continue from the game menu.'))
        except queue.Empty:
            if self.running and inside('cache/active-session.json').exists():
                self.status.set(tr('Spiel läuft. Beenden im Spielmenü oder mit „Spiel beenden“.', 'Game running. Exit from the game menu or select Stop game.'))
        self.root.after(200, self.poll)

    def stop_game(self):
        if request_stop(): self.status.set(tr('Das Spiel wird regulär geschlossen …', 'Closing the game normally…'))
        else: self.status.set(tr('Der Start wird noch vorbereitet. Bitte kurz warten.',"Still preparing the game. Please wait."))

    def close(self):
        if self.running: self.root.iconify()
        else: self.root.destroy()


if __name__ == '__main__':
    try:
        app = Launcher(); app.root.mainloop()
    except Exception as error:
        # pythonw has no console; surface startup failures instead of vanishing.
        import ctypes
        ctypes.windll.user32.MessageBoxW(None, str(error), tr('Harry Potter I — Starter', 'Harry Potter I — Launcher'), 0x10)
