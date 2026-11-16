"""Language selection for HP1's own interfaces; original localization is untouched."""
import configparser
from project_paths import inside


def english():
    prefs = configparser.ConfigParser(interpolation=None, strict=False)
    prefs.read(inside('config/vr-preferences.ini'), encoding='utf-8-sig')
    try:
        choice = prefs.getint('VR', 'Language', fallback=0)
    except ValueError:
        choice = 0
    if choice in (1, 2):
        return choice == 2
    game = configparser.ConfigParser(interpolation=None, strict=False)
    game.read(inside('Harry Potter TM/System/Default.ini'), encoding='cp1252')
    return game.get('Engine.Engine', 'Language', fallback='int').lower() not in ('ger', 'de')


def tr(de, en):
    return en if english() else de
