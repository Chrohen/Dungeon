#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"

struct FGridModel;

class FGridPathfinder
{
public:
	// Все клетки, до которых можно дойти (Дейкстра по стоимости тайлов).
	// Nodes включает Origin (Cost = 0, bCanStop = false) и клетки союзников, через которые
	// можно пройти (bCanStop = false). Клетки с юнитами другой команды не входят в Nodes
	// и блокируют путь. Если Origin вне поля, Nodes пуст
	static FReachableArea ComputeReachable(const FGridModel& Model, const FPathQuery& Query);

	// Путь от Origin до Target по Parent. OutPath очищается и заполняется клетками по порядку
	// шагов: Origin в путь НЕ входит (каждый элемент - один шаг), Target - последний элемент.
	// Для Target == Origin возвращает true и пустой путь. Для клетки, которой нет в Nodes,
	// возвращает false и пустой путь
	static bool BuildPath(const FReachableArea& Area, FIntPoint Target, TArray<FIntPoint>& OutPath);
};
