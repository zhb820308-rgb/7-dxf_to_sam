#ifndef FORMCALCUTILS_H
#define FORMCALCUTILS_H

#include <cowListDouble.h>
#include <QVector>

inline cowListDouble convertListDouble(const QVector<double>& vector)
{
    cowListDouble list(cowListConstruct::cowEstimateLength, vector.size());
    for(double value : vector)
    {
        list.Append(value);
    }
    return list;
}

#endif // FORMCALCUTILS_H
