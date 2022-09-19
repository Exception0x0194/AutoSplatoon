#include "autosplatoon.h"
#include "ui_autosplatoon.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QMessageBox>
#include <QPair>
#include <QProcess>
#include <QQueue>
#include <QRadioButton>
#include <QSerialPortInfo>
#include <QTextStream>
#include <QThread>
#include <QVector>
#include <QtDebug>

AutoSplatoon::AutoSplatoon(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::AutoSplatoon) {
  ui->setupUi(this);
  fillSerialPorts();

  signalMapper = new QSignalMapper(this);

  setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

  setFixedSize(this->width(), this->height());

  setWindowTitle("AutoSplatoon");

  ui->intervalBox->setValue(70);
  ui->rowBox->setValue(0);
  ui->columnBox->setValue(0);

  manControl2 = new ManualControl();
  manControl2->setAttribute(Qt::WA_DeleteOnClose);
  connect(manControl2, SIGNAL(buttonAction(quint64, bool)), this,
          SLOT(recieveButtonAction(quint64, bool)));
  connect(manControl2, SIGNAL(manControlDeletedSignal()), this,
          SLOT(manControlDeletedSignal()));
}

AutoSplatoon::~AutoSplatoon() { delete ui; }

void AutoSplatoon::fillSerialPorts() {
  QList<QSerialPortInfo> serialList = QSerialPortInfo::availablePorts();
  ui->serialPortsBox->clear();
  serialPorts.clear();
  for (const QSerialPortInfo &serialPortInfo : serialList) {
    serialPorts.append(serialPortInfo.systemLocation());
  }
  ui->serialPortsBox->addItems(serialPorts);
}

void AutoSplatoon::on_serialRefreshButton_clicked() { fillSerialPorts(); }

void AutoSplatoon::manControlDeletedSignal() { manControlDeleted = true; }

void AutoSplatoon::on_uploadButton_clicked() {
  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Open Image"), QDir::homePath(), tr("(*.bmp)"));
  QImage tmp = QImage(fileName);
  if (tmp.width() != 320 || tmp.height() != 120) {
    return;
  }
  haltFlag = true;
  // startFlag = false;
  pauseFlag = false;
  image = QImage(fileName);
  QGraphicsScene *scene = new QGraphicsScene;
  scene->addPixmap(QPixmap::fromImage(image));
  ui->graphicsView->setScene(scene);
  ui->graphicsView->show();
  ui->startButton->setEnabled(true);
  ui->label->setEnabled(true);
  ui->label_2->setEnabled(true);
  ui->label_4->setEnabled(true);
  ui->intervalBox->setEnabled(true);
  ui->rowBox->setEnabled(true);
  ui->columnBox->setEnabled(true);

  QFile pathfile("./path.txt");
  pathfile.open(QIODevice::WriteOnly | QIODevice::Truncate);
  QTextStream pathfileStream(&pathfile);

  operations.clear();
  int dis = 0, width = image.width(), height = image.height();
  QPair<int, int> pos{0, 0};
  QQueue<QPair<int, int>> posTodo{};
  QVector<QVector<int>> img(width, QVector<int>(height));
  for (int x = 0; x < width; x++)
    for (int y = 0; y < height; y++)
      img[x][y] = qGray(image.pixel(x, y));
  while (1) {
    int posx = pos.first, posy = pos.second;
    dis++;
    for (int dis1 = 0; dis1 < dis; dis1++) {
      int dis2 = dis - dis1;
      if (posx + dis1 < width && posy + dis2 < height)
        posTodo.push_back({posx + dis1, posy + dis2});
      if (posx + dis2 < width && posy - dis1 >= 0)
        posTodo.push_back({posx + dis2, posy - dis1});
      if (posx - dis1 >= 0 && posy - dis2 >= 0)
        posTodo.push_back({posx - dis1, posy - dis2});
      if (posx - dis2 >= 0 && posy + dis1 < height)
        posTodo.push_back({posx - dis2, posy + dis1});
    }
    if (posTodo.isEmpty()) {
      QMessageBox box;
      box.setText("打印路径长度：" + QString::number(operations.length()));
      box.exec();
      break;
    }
    while (!posTodo.isEmpty()) {
      QPair<int, int> nextPos = posTodo.front();
      int nextx = nextPos.first, nexty = nextPos.second;
      posTodo.pop_front();
      if (img[nextx][nexty] < 128) {
        int movex = nextx - posx, movey = nexty - posy;
        char opx = movex > 0 ? 'r' : 'l', opy = movey > 0 ? 'd' : 'u';
        for (int i = 0; i < abs(movex); i++)
          operations.push_back(opx);
        for (int i = 0; i < abs(movey); i++)
          operations.push_back(opy);
        operations.push_back('a');

        img[nextx][nexty] = 255;
        pos = {nextx, nexty};
        posTodo.clear();
        dis = 0;
      }
    }
  }
}

void AutoSplatoon::recieveButtonAction(quint64 action, bool temporary) {
  emit sendButtonAction(action, temporary);
}

void AutoSplatoon::handleSerialStatus(uint8_t status) {
  if (status == CONNECTING) {
    ui->serialStatusLabel->setText("连接中");
    ui->serialPortsBox->setEnabled(false);
    ui->serialRefreshButton->setEnabled(false);
    ui->uploadButton->setEnabled(false);
    // ui->forceVanillaConnection->setEnabled(false);
    ui->serialConnectButton->setText("断开连接");
  } else if (status == CONNECTED_OK) {
    ui->serialStatusLabel->setText("已连接");
    ui->uploadButton->setEnabled(true);
    ui->flashButton->setEnabled(false);
    // connect(this, SIGNAL(), serialController,
    // SLOT(recieveButtonAction(quint64, bool)));
  } else if (status == CHOCO_SYNCED_JC_L) {
    ui->serialStatusLabel->setText("Connected as Left JoyCon!");
  } else if (status == CHOCO_SYNCED_JC_R) {
    ui->serialStatusLabel->setText("Connected as Right JoyCon!");
  } else if (status == CHOCO_SYNCED_PRO) {
    ui->serialStatusLabel->setText("已连接");
    ui->uploadButton->setEnabled(true);
    ui->flashButton->setEnabled(false);
  } else if (status == CONNECTION_FAILED) {
    ui->serialStatusLabel->setText("连接失败");
  } else {
    ui->serialStatusLabel->setText("断连");
    ui->serialPortsBox->setEnabled(true);
    ui->serialRefreshButton->setEnabled(true);
    ui->uploadButton->setEnabled(false);
    ui->flashButton->setEnabled(true);
    ui->serialConnectButton->setText("连接");
    ui->serialConnectButton->setEnabled(true);
    ui->startButton->setEnabled(false);
    ui->pauseButton->setEnabled(false);
    ui->haltButton->setEnabled(false);
    // ui->forceVanillaConnection->setEnabled(true);
  }
}

void AutoSplatoon::createSerial() {
  if (ui->serialPortsBox->currentText().isEmpty() == false) {
    if (!ui->serialPortsBox->isEnabled()) {
      ui->serialConnectButton->setEnabled(false);
      // ui->forceVanillaConnection->setEnabled(false);
      serialController->deleteLater();
    } else {
      serialController = new SerialController(this);
      connect(serialController, SIGNAL(reportSerialStatus(uint8_t)), this,
              SLOT(handleSerialStatus(uint8_t)));
      serialController->openAndSync(ui->serialPortsBox->currentText());
    }
  }
}

void AutoSplatoon::on_serialConnectButton_clicked() { createSerial(); }

QProcess process;
QString output;

void AutoSplatoon::on_readoutput() {
  output.append(QString(process.readAllStandardOutput().data()));
}

void AutoSplatoon::on_flashButton_clicked() {
  ui->serialConnectButton->setEnabled(false);
  ui->flashButton->setEnabled(false);
  ui->serialPortsBox->setEnabled(false);
  ui->serialRefreshButton->setEnabled(false);
  ui->manualButton->setEnabled(false);
  ui->serialStatusLabel->setText("烧录中");

  QElapsedTimer timer;
  timer.start();
  QString cmd = QApplication::applicationDirPath();
  cmd += "/esptool.exe";
  qDebug() << cmd;
  QStringList arg;
  arg << "--baud";
  arg << "230400";
  arg << "write_flash";
  arg << "0x0";
  arg << QApplication::applicationDirPath() + "/PRO-UART0.bin";
  process.start(cmd, arg);
  connect(&process, SIGNAL(readyReadStandardOutput()), this,
          SLOT(on_readoutput()));
  QEventLoop loop;
  connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)), &loop,
          SLOT(quit()));
  loop.exec();
  process.kill();
  qDebug() << output;
  int flag = output.indexOf("100 %");
  if (flag == -1)
    ui->serialStatusLabel->setText("烧录失败");
  else
    ui->serialStatusLabel->setText("烧录成功");
  ui->serialConnectButton->setEnabled(true);
  ui->flashButton->setEnabled(true);
  ui->serialPortsBox->setEnabled(true);
  ui->serialRefreshButton->setEnabled(true);
  ui->manualButton->setEnabled(true);

  output = "";
}

void AutoSplatoon::executeTask() {
  while (!operations.isEmpty() && haltFlag) {
    char op = operations.front();
    switch (op) {
    case 'l':
      manControl2->sendCommand("Dl", interval);
      break;
    case 'r':
      manControl2->sendCommand("Dr", interval);
      break;
    case 'u':
      manControl2->sendCommand("Du", interval);
      break;
    case 'd':
      manControl2->sendCommand("Dd", interval);
      break;
    case 'a':
      manControl2->sendCommand("A", interval);
      break;
    default:
      break;
    }
    operations.pop_front();
  }
  on_haltButton_clicked();
}

void AutoSplatoon::on_startButton_clicked() {
  ui->startButton->setEnabled(false);
  ui->pauseButton->setEnabled(true);
  ui->haltButton->setEnabled(true);
  ui->uploadButton->setEnabled(false);

  ui->label->setEnabled(false);
  //    ui->label_2->setEnabled(false);
  //    ui->label_4->setEnabled(false);
  ui->label_2->setText("当前行数");
  ui->label_4->setText("当前列数");
  ui->intervalBox->setEnabled(false);
  ui->rowBox->setEnabled(false);
  ui->columnBox->setEnabled(false);

  interval = ui->intervalBox->value();
  row = ui->rowBox->value();
  column = ui->columnBox->value();

  // column = 0; row = 0;
  // startFlag = true;
  haltFlag = false;

  executeTask();
}

void AutoSplatoon::on_pauseButton_clicked() {
  if (!pauseFlag) {
    ui->pauseButton->setText("继续");
    pauseFlag = true;
  } else {
    pauseFlag = false;
    ui->pauseButton->setText("暂停");
  }
}

void AutoSplatoon::on_haltButton_clicked() {
  ui->pauseButton->setEnabled(false);
  ui->haltButton->setEnabled(false);
  ui->uploadButton->setEnabled(true);
  ui->startButton->setEnabled(false);
  ui->label->setEnabled(false);
  ui->label_2->setEnabled(false);
  ui->label_4->setEnabled(false);
  ui->intervalBox->setEnabled(false);
  ui->rowBox->setEnabled(false);
  ui->columnBox->setEnabled(false);
  ui->pauseButton->setText("暂停");
  // startFlag = false;
  pauseFlag = false;
  haltFlag = true;
  column = 0;
  row = 0;

  QGraphicsScene *scene = new QGraphicsScene;
  scene = NULL;
  ui->graphicsView->setScene(scene);
  ui->graphicsView->show();
}

void AutoSplatoon::on_manualButton_clicked() {
  if (manControlDeleted) {
    manControlDeleted = false;
    manControl1 = new ManualControl();
    manControl1->setAttribute(Qt::WA_DeleteOnClose);
    manControl1->show();
    connect(manControl1, SIGNAL(buttonAction(quint64, bool)), this,
            SLOT(recieveButtonAction(quint64, bool)));
    connect(manControl1, SIGNAL(manControlDeletedSignal()), this,
            SLOT(manControlDeletedSignal()));
  }
}
