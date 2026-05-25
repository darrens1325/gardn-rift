import math

RARITY_COUNT = 9


def loot_table_gen(n):
    """
    生成掉落概率表
    :param n: float，取值范围 [0, 1]，用于调节整体掉落率
    :return: list of list，9行7列的二维列表，每行概率之和为1
    """
    # 参数校验
    if not isinstance(n, (int, float)) or n < 0 or n > 1:
        raise ValueError("参数 n 必须为 [0,1] 之间的数字")

    drops = [
        0,
        0.8589559816476924,
        0.9963889387113232,
        0.9998247626379139,
        0.9999965538342435,
        0.9999999896581699,
        0.9999999999656417,
        1,
    ]
    mobs = [
        60000,
        15000,
        1500,
        100,
        5,
        0.1,
        0.0005,
        0.0000025,
        0.000001,
    ]

    # 创建 9x7 的表格，初始化为 0.0
    table = [[0.0] * 7 for _ in range(RARITY_COUNT)]

    # Capped variant: matches NORMAL-map behavior in
    # Shared/StaticData.cc's _loot_table_gen. BR maps use a separate
    # cap-free formula (see .ocr_work/build_inl.py and
    # _loot_table_gen_br in StaticData.cc).
    for mob in range(RARITY_COUNT):
        cap = max(1, mob)
        for drop in range(cap + 1):
            if drop > 6:
                break
            start = drops[drop]
            end = 1 if drop == cap else drops[drop + 1]
            exponent = 300000.0 / mobs[mob]
            base1 = n * start + (1 - n)
            base2 = n * end + (1 - n)
            pow_term1 = base1 ** exponent
            pow_term2 = base2 ** exponent
            table[mob][drop] = pow_term2 - pow_term1

    return table


# 测试调用示例
if __name__ == '__main__':
    # 可以自行修改 n 的值：0.001 / 0.01 / 0.05 / 0.1 / 0.25 / 0.75 / 1
    result = loot_table_gen(0.5)
    for i, row in enumerate(result):
        print(f"mob{i}: {[round(x, 6) for x in row]}")
