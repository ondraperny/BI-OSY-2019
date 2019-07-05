#ifndef __PROGTEST__

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "progtest_solver.h"
#include "sample_tester.h"

using namespace std;
#endif /* __PROGTEST__ */


class Buffer {
    deque<pair<ACustomer, AOrderList>> bufferDeque;
    mutex bufferMutex;
    condition_variable cv_bufferIsFull;
    condition_variable cv_bufferIsEmpty;
    unsigned bufferSize;

public:
    Buffer(){
        bufferSize = 30;
    }

    virtual void insert(ACustomer aCust, AOrderList aOrderL) {
        unique_lock<mutex> ul(bufferMutex);

        cv_bufferIsFull.wait(ul, [this]() { return (bufferDeque.size() < bufferSize); });
        bufferDeque.emplace_back(make_pair(aCust, aOrderL));
        cv_bufferIsEmpty.notify_one();
//        cout << "   Pridan do bufferu(id) " << aOrderL->m_MaterialID << " Pocet prvku ve fronte: " << bufferDeque.size()
//             << endl;
    }

    virtual pair<ACustomer, AOrderList> remove() {
        unique_lock<mutex> ul(bufferMutex);

        pair<ACustomer, AOrderList> aCustAndOrderL;

        cv_bufferIsEmpty.wait(ul, [this]() { return (!bufferDeque.empty()); });
        aCustAndOrderL = bufferDeque.front();
        bufferDeque.pop_front();

        cv_bufferIsFull.notify_one();
//        cout << "   Vyndan  z bufferu(id) " << aCustAndOrderL.second->m_MaterialID << " Pocet prvku ve fronte: "
//             << bufferDeque.size() << endl;
        return aCustAndOrderL;
    }
};

class CWeldingCompany {
public:
    static void SeqSolve(APriceList priceList,
                         COrder &order);

    void AddProducer(AProducer prod);

    void AddCustomer(ACustomer cust);

    void AddPriceList(AProducer prod,
                      APriceList priceList);

    void Start(unsigned thrCount);

    void Stop(void);

    void CustomerServingThread(const ACustomer &customer);

    void WorkingThread();

    bool isMaterialEvaluated(unsigned matID);

private:

    vector<ACustomer> Customers;
    vector<AProducer> Producers;
    Buffer buffer;

    vector<thread> workThreads;
    vector<thread> serviceThreads;

    mutex addToPrices;
    mutex updatePriceListMutex;

    condition_variable cv_materialAdded;

    map<unsigned, pair<unsigned, APriceList>> prices;
};

bool CWeldingCompany::isMaterialEvaluated(unsigned matID) {
    if (!prices.count(matID))
        return false;
    return prices[matID].first >= Producers.size();
}

void CWeldingCompany::CustomerServingThread(const ACustomer &customer) {
    AOrderList currentOrder;

    while (true) {
        currentOrder = customer->WaitForDemand();
        if (currentOrder.get() == nullptr)
            break;

        // check if given material is already in database
        if (!prices.count(currentOrder->m_MaterialID)) {
            for (auto &prod : Producers) {
                prod->SendPriceList(currentOrder->m_MaterialID);
            }
        }

        unique_lock<mutex> lock(updatePriceListMutex);
        // wait until all Producers evaluate material
        while (!isMaterialEvaluated(currentOrder->m_MaterialID)) cv_materialAdded.wait(lock);
        lock.unlock();

        buffer.insert(customer, currentOrder);
    }
}

void CWeldingCompany::WorkingThread() {
    pair<ACustomer, AOrderList> aCustAndOrderL;

    while (true) {
        aCustAndOrderL = buffer.remove();
        // stop function fill buffer with nullptr causing WorkingThread to terminate on next if
        if (aCustAndOrderL.first == nullptr)
            break;

        if(!prices.count(aCustAndOrderL.second->m_MaterialID))
            cout << "Material neni v databazi takze se neco podelalo" << endl;

        ProgtestSolver(aCustAndOrderL.second->m_List, prices[aCustAndOrderL.second->m_MaterialID].second);
        aCustAndOrderL.first->Completed(aCustAndOrderL.second);
    }
}

void CWeldingCompany::AddPriceList(AProducer prod,
                                   APriceList priceList) {

    unique_lock<mutex> lock(addToPrices);

    if (!prices.count(priceList->m_MaterialID)) {
        APriceList tmp(new CPriceList(priceList->m_MaterialID));
        if(!priceList->m_List.empty())
            tmp->Add(priceList->m_List.front());
        prices[priceList->m_MaterialID] = make_pair(0, tmp);
    }

//    // print vsechny materialy
//    cout << "Materialy v databazi: ";
//    for (auto &mat : prices)
//        cout << mat.first << " ";
//    cout << endl;

    for (auto &newProd : priceList->m_List) {
        for (auto &currentProd : prices[priceList->m_MaterialID].second->m_List) {
            if ((newProd.m_W == currentProd.m_W && newProd.m_H == currentProd.m_H) ||
                (newProd.m_W == currentProd.m_H && newProd.m_H == currentProd.m_W)) {
                if (newProd.m_Cost < currentProd.m_Cost)
                    currentProd = newProd;
                break;
            }
        }
        if(!prices.count(priceList->m_MaterialID))
            cout << "Material neni v databazi takze se neco podelalo" << endl;
        prices[priceList->m_MaterialID].second->Add(newProd);
    }
    cout << "Pocet evaluaci: " << ++prices[priceList->m_MaterialID].first << " , pro matID: " << priceList->m_MaterialID
         << endl;

    if (isMaterialEvaluated(priceList->m_MaterialID)) {
        cv_materialAdded.notify_all();
        cout << "Vyslana notifykace o kompletni pricelistu" << endl;
    }
}

// complete
void CWeldingCompany::SeqSolve(APriceList priceList, COrder &order) {
    vector<COrder> orders{order};
    ProgtestSolver(orders, priceList);
    order = orders.front();
}

// complete
void CWeldingCompany::AddProducer(AProducer prod) {
    Producers.push_back(prod);
    std::cout << "Pridan producent, celkovy pocet producentu: " << Producers.size() << endl;
}

// complete
void CWeldingCompany::AddCustomer(ACustomer cust) {
    Customers.push_back(cust);
    std::cout << "Pridan zakaznik , celkovy pocet zakazniku : " << Customers.size() << endl;
}

// start all threads - finding customer requests, quering suppliers, solving customer requests
// once all those threads are started method return to caller - doesn't wait until created threads finish
void CWeldingCompany::Start(unsigned thrCount) {
    std::cout << "Pocet pracovnich vlaken: " << thrCount << endl;

    for (unsigned i = 0; i < Customers.size(); ++i) {
        serviceThreads.emplace_back(thread(&CWeldingCompany::CustomerServingThread, this, Customers[i]));
        std::cout << "Spusteno obsluzne vlakno pro zakaznika" << endl;
    }

    for (unsigned i = 0; i < thrCount; ++i) {
        workThreads.emplace_back(thread(&CWeldingCompany::WorkingThread, this));
        std::cout << "Spusteno pracovni vlakno" << endl;
    }
}

// wait as long as customers sending requests, until their requests are fulfilled
// then end all running thread, when no threads are running return to caller
void CWeldingCompany::Stop(void) {
//    cout << "Prikaz k ukonceni" << endl;
    for (auto &servingThr : serviceThreads) {
        servingThr.join();
    }

    // fill buffer with nullptrs
    for (unsigned i = 0; i < workThreads.size(); i++) {
        ACustomer tmp1;
        AOrderList tmp2;
        buffer.insert(tmp1, tmp2);
    }

    for (auto &workingThr : workThreads) {
        workingThr.join();
    }
}


//-------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__

int main(void) {
    using namespace std::placeholders;
    CWeldingCompany  test;

    AProducer p1 = make_shared<CProducerSync> ( bind ( &CWeldingCompany::AddPriceList, &test, _1, _2 ) );
    AProducerAsync p2 = make_shared<CProducerAsync> ( bind ( &CWeldingCompany::AddPriceList, &test, _1, _2 ) );
    test . AddProducer ( p1 );
    test . AddProducer ( p2 );
    test . AddCustomer ( make_shared<CCustomerTest> ( 2 ) );
    p2 -> Start ();
    test . Start ( 1 );
    test . Stop ();
    p2 -> Stop ();
    return 0;
}

#endif /* __PROGTEST__ */
