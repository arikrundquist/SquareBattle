#!/usr/bin/python

import os

from PyQt6 import QtWidgets, QtGui

class TeamPicker(QtWidgets.QHBoxLayout):
    def add_to_end(self):
        if self is not self.parent().children()[-1]:
            return
        self.parent().addLayout(TeamPicker(self.all_teams))

    def remove(self):
        while self.count():
            widget = self.takeAt(0).widget()
            if widget is not None:
                widget.setParent(None)
                widget.deleteLater()
        self.deleteLater()

    def update_color(self):
        self.team_color = QtWidgets.QColorDialog.getColor()
        self.canvas.fill(self.team_color)
        self.team_color_lbl.setPixmap(self.canvas)

    def update_value(self, index):
        if index == 0:
            self.remove()
        else:
            self.add_to_end()

    def __init__(self, all_teams: list[str], default_color: tuple=(0, 0, 0)):
        super().__init__()

        self.all_teams = all_teams

        all_teams = list(map(lambda p: os.path.basename(p), all_teams))

        self.team_color_lbl = QtWidgets.QLabel()
        self.canvas = QtGui.QPixmap(20, 20)
        self.team_color = QtWidgets.QColorDialog.getColor() if default_color is None else QtGui.QColor.fromRgb(*default_color)
        self.canvas.fill(self.team_color)
        self.team_color_lbl.setPixmap(self.canvas)
        self.team_color_changer = QtWidgets.QToolButton()
        
        self.team_color_changer.setText("...")

        self.team_color_changer.clicked.connect(self.update_color)
        
        if len(all_teams) == 0:
            raise Exception("no teams found")

        if len(all_teams) != 1:
            self.team_select = QtWidgets.QComboBox()
            self.team_select.addItems([""] + all_teams)
            self.team_select.currentIndexChanged.connect(self.update_value)
            self.get_team = lambda: self.all_teams[self.team_select.currentIndex()-1]
        else:
            self.team_select = QtWidgets.QLabel(all_teams[0])
            self.get_team = lambda: self.all_teams[0]

        self.addWidget(self.team_select)
        self.addWidget(self.team_color_lbl)
        self.addWidget(self.team_color_changer)

        self.get_color = lambda: (self.team_color.red(), self.team_color.green(), self.team_color.blue())

class Window(QtWidgets.QDialog):
    def __init__(self, title: str, all_teams: list[str]):
        super().__init__(parent=None)
        self.setWindowTitle(title)

        self.all_teams = sorted(all_teams)
        
        all = QtWidgets.QVBoxLayout()
        teams = QtWidgets.QVBoxLayout()
        self.teams = teams

        self.seed = QtWidgets.QLineEdit("0")
        self.seed.setValidator(QtGui.QIntValidator())
        
        self.teams.addLayout(TeamPicker(["background"], default_color=(255, 255, 255)))
        self.teams.addLayout(TeamPicker(all_teams))

        buttons = QtWidgets.QHBoxLayout()
        battle = QtWidgets.QPushButton("Battle!")
        buttons.addWidget(battle)

        all.addWidget(self.seed)
        all.addLayout(teams)
        all.addLayout(buttons)
        self.setLayout(all)

        battle.clicked.connect(self.battle)

    def battle(self):
        def format_color(color):
            return "%d %d %d" % color
        background = self.teams.children()[0]
        def team_string(team):
            return f' -t "{team.get_team()}" {format_color(team.get_color())}'
        params = f"-s {self.seed.text()} -b {format_color(background.get_color())}" + ''.join(map(team_string, self.teams.children()[1:-1]))
        cmd = f"./backend {params} &"
        os.system(cmd)

def main():

    def get_teams():
        return [team for team in [os.path.join("./teams", f) for f in os.listdir("./teams")]  if os.path.isdir(team)]

    app = QtWidgets.QApplication([])
    window = Window("Square Battle", get_teams())
    window.show()
    return app.exec()

if __name__ == "__main__":
    from sys import exit
    exit(main())
