#include <iostream>
#include <sstream>
#include <QtCore>
#include <QtCore/QCoreApplication>
#include "model/model.h"
#include "glow/Dispatcher.h"


#define VERSION_STRING "0.0.2"


// =====================================================
//
// DOM tree creation
//
// =====================================================

std::string identOf(std::string baseStr, int n)
{
   auto stream = std::stringstream();
   stream << baseStr << "-" << n;
   return stream.str();
}

void createOneToN(model::Node* router, int nodeNumber, glow::Dispatcher* dispatcher)
{
   auto const signalCount = 200;

   auto oneToN = new model::Node(nodeNumber, router, "oneToN");
   oneToN->setDescription("Linear 1:N");

   auto labels = new model::Node(1, oneToN, "labels");
   auto targetLabels = new model::Node(1, labels, "targets");
   auto sourceLabels = new model::Node(2, labels, "sources");

   for(int number = 0; number < signalCount; number++)
   {
      auto targetParameter = new model::StringParameter(number, targetLabels, identOf("t", number), dispatcher);
      targetParameter->setValue(identOf("SDI-T", number));

      auto sourceParameter = new model::StringParameter(number, sourceLabels, identOf("s", number), dispatcher);
      sourceParameter->setValue(identOf("SDI-S", number));
   }

   auto matrix = new model::matrix::OneToNLinearMatrix(2, oneToN, "matrix", dispatcher, signalCount, signalCount);
   matrix->setLabelsPath(labels->path());

   for each(auto target in matrix->targets())
   {
      auto source = matrix->getSource(target->number());
      matrix->connect(target, &source, &source + 1, nullptr);
   }
}

void createNToN(model::Node* router, int nodeNumber, glow::Dispatcher* dispatcher)
{
   auto nToN = new model::Node(nodeNumber, router, "nToN");
   nToN->setDescription("Non-Linear N:N");

   auto labels = new model::Node(1, nToN, "labels");
   auto targetLabels = new model::Node(1, labels, "targets");
   auto sourceLabels = new model::Node(2, labels, "sources");

   auto parameters = new model::Node(2, nToN, "parameters");
   auto targetParams = new model::Node(1, parameters, "targets");
   auto sourceParams = new model::Node(2, parameters, "sources");
   auto xpointParams = new model::Node(3, parameters, "connections");

   auto targets = model::matrix::Signal::Vector();
   auto sources = model::matrix::Signal::Vector();
   auto number = 0;

   for(int index = 0; index < 4; index++)
   {
      number += 3;

      auto targetLabel = new model::StringParameter(number, targetLabels, identOf("t", number), dispatcher);
      targetLabel->setValue(identOf("AES-T", number));
      targets.insert(targets.end(), new model::matrix::Signal(number));

      auto targetNode = new model::Node(number, targetParams, identOf("t", number));
      (new model::IntegerParameter(1, targetNode, "targetGain", dispatcher, -128, 15))->setValue(-64);
      (new model::StringParameter(2, targetNode, "targetMode", dispatcher))->setValue("something");

      auto sourceLabel = new model::StringParameter(number, sourceLabels, identOf("s", number), dispatcher);
      sourceLabel->setValue(identOf("AES-S", number));

      sources.insert(sources.end(), new model::matrix::Signal(number));
      auto sourceNode = new model::Node(number, sourceParams, identOf("s", number));
      (new model::IntegerParameter(1, sourceNode, "sourceGain", dispatcher, -128, 15))->setValue(-64);
   }

   auto const gainParameterNumber = 1;

   for each(auto target in targets)
   {
      auto targetNode = new model::Node(target->number(), xpointParams, identOf("t", target->number()));

      for each(auto source in sources)
      {
         auto sourceNode = new model::Node(source->number(), targetNode, identOf("s", source->number()));
         (new model::IntegerParameter(gainParameterNumber, sourceNode, "gain", dispatcher, -128, 15))->setValue(0);
      }
   }

   auto matrix = new model::matrix::NToNNonlinearMatrix(3, nToN, "matrix", dispatcher, targets.begin(), targets.end(), sources.begin(), sources.end());
   matrix->setLabelsPath(labels->path());
   matrix->setParametersPath(parameters->path());
   matrix->setGainParameterNumber(gainParameterNumber);

   for each(auto target in matrix->targets())
   {
      if(target->number() % 2 == 0)
      {
         auto source = matrix->getSource(target->number());
         matrix->connect(target, &source, &source + 1, nullptr);
      }
   }
}

void createDynamic(model::Node* router, int nodeNumber, glow::Dispatcher* dispatcher)
{
   auto const signalCount = 2000;

   auto dynamic = new model::Node(nodeNumber, router, "dynamic");
   dynamic->setDescription("Linear N:N with Dynamic Parameters");

   auto labels = new model::Node(1, dynamic, "labels");
   auto targetLabels = new model::Node(1, labels, "targets");
   auto sourceLabels = new model::Node(2, labels, "sources");

   for(int number = 0; number < signalCount; number++)
   {
      auto targetParameter = new model::StringParameter(number, targetLabels, identOf("t", number), dispatcher);
      targetParameter->setValue(identOf("SDI-T", number));

      auto sourceParameter = new model::StringParameter(number, sourceLabels, identOf("s", number), dispatcher);
      sourceParameter->setValue(identOf("SDI-S", number));
   }

   auto matrix = new model::matrix::DynamicNToNLinearMatrix(2, dynamic, "matrix", dispatcher, signalCount, signalCount);
   matrix->setLabelsPath(labels->path());

   for each(auto target in matrix->targets())
   {
      auto source = matrix->getSource(target->number());
      matrix->connect(target, &source, &source + 1, nullptr);
   }
}

void createProductInfo(model::Node* router, int nodeNumber)
{
   auto productInfo = new model::Node(nodeNumber, router, "__productInfo");

   auto moniker = new model::StringParameter(1, productInfo, "moniker", nullptr);
   moniker->setReadOnly(true);
   moniker->setValue("Tiny Ember+ Router");

   auto author = new model::StringParameter(2, productInfo, "author", nullptr);
   author->setReadOnly(true);
   author->setValue("L-S-B Broadcast Technologies GmbH");

   auto version = new model::StringParameter(3, productInfo, "version", nullptr);
   version->setReadOnly(true);
   version->setValue(VERSION_STRING);
}

model::Element* createTree(glow::Dispatcher* dispatcher)
{
   auto root = model::Element::createRoot();
   auto router = new model::Node(1, root, "router");
   createProductInfo(router, 0);
   createOneToN(router, 1, dispatcher);
   createNToN(router, 2, dispatcher);
   createDynamic(router, 3, dispatcher);

   return root;
}


// =====================================================
//
// Qt console reading
//
// =====================================================

class ConsoleReaderThread : public QThread
{
   Q_OBJECT

protected:
   void run()
   {
      std::cout << "Tiny Ember+ Router v" << VERSION_STRING << " started. Press enter to quit..." << std::endl;

      fgetc(stdin);
   }
};

class Task : public QObject
{
   Q_OBJECT
public:
   Task(QObject *parent = 0) : QObject(parent) {}

public slots:
   void run()
   {
      ConsoleReaderThread* thread = new ConsoleReaderThread();
      thread->start();
      QObject::connect(thread, SIGNAL(finished()), parent(), SLOT(quit()));
   }
};

#include "main.moc"


// =====================================================
//
// Entry point
//
// =====================================================

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    auto dispatcher = glow::Dispatcher(&a, 9092);
    auto root = createTree(&dispatcher);
    dispatcher.setRoot(root);

    // Task will be deleted by the application object.
    auto task = new Task(&a);

    // run the task from the application event loop.
    QTimer::singleShot(0, task, SLOT(run()));

    auto result = a.exec();
    delete root;
    return result;
}